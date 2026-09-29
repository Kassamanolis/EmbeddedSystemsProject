#include "common.h"
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Data per WebSocket session for fragment reassembly
typedef struct {
    unsigned char *buf;
    size_t len;
    size_t cap;
} per_session_data;

typedef struct {
    circular_buffer *cb;
    volatile int session_active;
    volatile int backoff;
} producer_context;

static int assemble_message(per_session_data *psd, const void *in, size_t len) {

    // Guard against messages that are too large to handle. Reset buffer, return error
    if (psd->len + len > MAX_MSG_SIZE) { 
        psd ->len = 0;
        return -1;
    }
    if (psd->len + len > psd->cap) {
        size_t new_cap;

        if (psd->cap == 0) new_cap = 4096;
        else new_cap = psd->cap * 2;

        while (new_cap < psd->len + len) new_cap *= 2;
        unsigned char *grown_buf = (unsigned char *)realloc(psd->buf, new_cap);
        if (!grown_buf) return -1;
        psd->buf = grown_buf;
        psd->cap = new_cap;
    }

    memcpy(psd->buf + psd->len, in, len);
    psd->len += len;
    return 0;
}

static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len) {
    per_session_data *psd = (per_session_data *)user;
    producer_context *prod_ctx = (producer_context *)lws_context_user(lws_get_context(wsi));

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        fprintf(stderr, "Connected to Jetstream firehose.\n");
        prod_ctx->session_active = 1;
        prod_ctx->backoff = 1;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        // Guard against malformed/large messages, continue
        if (assemble_message(psd, in, len) != 0) { 
            break;
        }
        if (lws_is_final_fragment(wsi)) {
            cb_push(prod_ctx->cb, (const char *)psd->buf, psd->len);
            psd->len = 0;
        }
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        if (in != NULL) {
            fprintf(stderr, "Connection error: %s\n", (char *)in);
        } else {
            fprintf(stderr, "Connection error\n");
        }
        

        prod_ctx->session_active = 0;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        fprintf(stderr, "Connection closed.\n");
        prod_ctx->session_active = 0;
        break;

    case LWS_CALLBACK_WSI_DESTROY:
        if (psd && psd->buf) {
            free(psd->buf);
            psd->buf = NULL;
            psd->cap = 0;
            psd->len = 0;
        }
        break;

    default:
        break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "jetstream-protocol",
        .callback = callback,
        .per_session_data_size = sizeof(per_session_data),
        .rx_buffer_size = 0, // We handle message assembly distinctly
    },
    LWS_PROTOCOL_LIST_TERM
};

void *producer_main(void *arg) {
    shared_state *state = (shared_state *)arg;
    producer_context prod_ctx = { .cb = state->buffer, .session_active = 0, .backoff = 1 };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = &prod_ctx;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create lws context.\n");
        return NULL;
    }

    const int backoff_max = 30;

    while (global_running) {
        struct lws_client_connect_info ccinfo;
        memset(&ccinfo, 0, sizeof(ccinfo));
        ccinfo.context = context;
        ccinfo.address = WS_HOST;
        ccinfo.port = WS_PORT;
        ccinfo.path = WS_PATH;
        ccinfo.host = ccinfo.address;
        ccinfo.origin = ccinfo.address;
        ccinfo.protocol = protocols[0].name;
        ccinfo.ssl_connection = LCCSCF_USE_SSL;

        prod_ctx.session_active = 1;
        struct lws *wsi = lws_client_connect_via_info(&ccinfo);

        // If initial connection fails,retry with exponential backoff
        if (!wsi) {
            fprintf(stderr, "lws_client_connect_via_info failed, "
                            "retry in %ds\n", prod_ctx.backoff);
            sleep(prod_ctx.backoff);
            prod_ctx.backoff = (prod_ctx.backoff * 2 < backoff_max) ? prod_ctx.backoff * 2 : backoff_max;
            continue;
        }

        while (global_running && prod_ctx.session_active) {
            lws_service(context, 1000);
        }

        if (!global_running) break;

        // If the connection is lost, retry with exponential backoff
        fprintf(stderr, "Reconnecting in %ds...\n", prod_ctx.backoff);
        sleep(prod_ctx.backoff);
        prod_ctx.backoff = (prod_ctx.backoff * 2 < backoff_max) ? prod_ctx.backoff * 2 : backoff_max;
    }

    lws_context_destroy(context);
    return NULL;
}
