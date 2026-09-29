#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <signal.h>

#define WS_HOST        "jetstream1.us-east.bsky.network"
#define WS_PATH        "/subscribe?wantedCollections=app.bsky.feed.post"
#define WS_PORT        443
#define LOG_FILENAME   "metrics_log.txt"

#define BUFFERSIZE 4096

#define MAX_MSG_SIZE (64 * 1024)

extern volatile sig_atomic_t global_running;

typedef struct {
    char *slots[BUFFERSIZE];
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} circular_buffer;

void cb_init(circular_buffer *cb);
void cb_destroy(circular_buffer *cb);

int cb_push(circular_buffer *cb, const char *data, size_t len);
char *cb_pop(circular_buffer *cb);
double cb_occupancy_pct(circular_buffer *cb);

typedef struct {
    unsigned long commit_count;
    unsigned long identity_count;
    unsigned long account_count;
    unsigned long info_count;
    unsigned long dropped_count;   
    pthread_mutex_t mutex;
} message_counters;

void counters_init(message_counters *c);
void counters_increase(message_counters *c, const char *kind);

void counters_reset( message_counters *c, int *commit, 
                        int *identity, int *account, int*info);

// Shared state structure to be passed to threads
typedef struct {
    circular_buffer *buffer;
    message_counters *counters;
} shared_state;

void *producer_main(void *arg);
void *consumer_main(void *arg);
void *monitor_main(void *arg);

#endif
