#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void cb_init(circular_buffer *cb) {
    memset(cb, 0, sizeof(*cb));
    pthread_mutex_init(&cb->mutex, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
}

void cb_destroy(circular_buffer *cb) {
    for (size_t i = 0; i < cb->count; i++) {
        size_t idx = (cb->head + i) % BUFFERSIZE;
        free(cb->slots[idx]);
    }
    pthread_mutex_destroy(&cb->mutex);
    pthread_cond_destroy(&cb->not_empty);
}

int cb_push(circular_buffer *cb, const char *data, size_t len) {
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        return -1;
    }
    memcpy(copy, data, len);
    copy[len] = '\0';

    pthread_mutex_lock(&cb->mutex);
    if (cb->count == BUFFERSIZE) {
        pthread_mutex_unlock(&cb->mutex);
        free(copy);
        return -1;
    }
    cb->slots[cb->tail] = copy;
    cb->tail = (cb->tail + 1) % BUFFERSIZE;
    cb->count++;
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->mutex);
    return 0;
}

char *cb_pop(circular_buffer *cb) {
    pthread_mutex_lock(&cb->mutex);
    while (cb->count == 0 && global_running) {
        pthread_cond_wait(&cb->not_empty, &cb->mutex);
    }
    if (cb->count == 0 && !global_running) {
        pthread_mutex_unlock(&cb->mutex);
        return NULL;
    }
    char *data = cb->slots[cb->head];
    cb->head = (cb->head + 1) % BUFFERSIZE;
    cb->count--;
    pthread_mutex_unlock(&cb->mutex);
    return data;
}

double cb_occupancy_pct(circular_buffer *cb) {
    pthread_mutex_lock(&cb->mutex);
    double pct = (100.0 * (double)cb->count) / (double)BUFFERSIZE;
    pthread_mutex_unlock(&cb->mutex);
    return pct;
}
