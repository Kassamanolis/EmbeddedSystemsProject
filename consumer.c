#include "common.h"
#include <cjson/cJSON.h>
#include <stdlib.h>

void *consumer_main(void *arg) {
    shared_state *state = (shared_state *)arg;

    while (1) {
        
         // event-driven 
        char *msg = cb_pop(state->buffer);
        if (msg == NULL) {  // cb->count == 0 && !g_running
            break;
        }

        cJSON *root = cJSON_Parse(msg);
        if (root) {
            const cJSON *kind = cJSON_GetObjectItemCaseSensitive(root, "kind");
            if (cJSON_IsString(kind) && kind->valuestring) {
                counters_increase(state->counters, kind->valuestring);
            } else {
                counters_increase(state->counters, "info"); // info/error
            }

            cJSON_Delete(root);

        } else {
            counters_increase(state->counters, "info"); // invalid JSON = info 
        }

        free(msg);
    }
    return NULL;
}
