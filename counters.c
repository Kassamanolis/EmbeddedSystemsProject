#include "common.h"
#include <string.h>

void counters_init(message_counters *c) {
    c->commit_count = 0;
    c->identity_count = 0;
    c->account_count = 0;
    c->info_count = 0;
    c->dropped_count = 0;
    pthread_mutex_init(&c->mutex, NULL);
}

void counters_increase(message_counters *c, const char *kind) {
    if (!kind) return;
    pthread_mutex_lock(&c->mutex);

    if (strcmp(kind, "commit") == 0) {
        c->commit_count++;
    } else if (strcmp(kind, "identity") == 0) {
        c->identity_count++;
    } else if (strcmp(kind, "account") == 0) {
        c->account_count++;
    } else {
        c->info_count++;
    }
    pthread_mutex_unlock(&c->mutex);
}

void counters_reset( message_counters *c, int *commit, 
                        int *identity, int *account, int*info) {    

    pthread_mutex_lock(&c->mutex);
    *commit = c->commit_count;
    *identity = c->identity_count;
    *account = c->account_count;
    *info = c->info_count;

    c->commit_count = 0;
    c->identity_count = 0;
    c->account_count = 0;
    c->info_count = 0;
    pthread_mutex_unlock(&c->mutex);
}
