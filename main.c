#include "common.h"
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

volatile sig_atomic_t global_running = 1;

int main(void) {
    circular_buffer buffer;
    message_counters counters;
    cb_init(&buffer);
    counters_init(&counters);
 
    shared_state state;
    state.buffer = &buffer;
    state.counters = &counters;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_t producer_tid, consumer_tid, logger_tid;

    pthread_create(&producer_tid, NULL, producer_main, &state);
    pthread_create(&consumer_tid, NULL, consumer_main, &state);
    pthread_create(&logger_tid, NULL, monitor_main, &state);
    
    int sig_received;
    sigwait(&set, &sig_received);
    fprintf(stderr, "\nSignal received (%d), shutting down...\n",
            sig_received);

    global_running = 0;

    // Wake up any threads waiting on the condition variable
    pthread_mutex_lock(&state.buffer->mutex);
    pthread_cond_broadcast(&state.buffer->not_empty);
    pthread_mutex_unlock(&state.buffer->mutex);

    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);
    pthread_join(logger_tid, NULL);

    fprintf(stderr, "All threads terminated cleanly.\n");
    fprintf(stderr, "Messages dropped due to full buffer: %lu\n", state.counters->dropped_count);

    cb_destroy(state.buffer);
    pthread_mutex_destroy(&state.counters->mutex);

    return 0;
}
