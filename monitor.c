#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} cpu_stats;

static int read_cpu_stats(cpu_stats *out) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    //"cpu user nice system idle iowait irq softirq steal guest guest_nice" , guest and guest_nice do not exist in a Raspberry Pi 
    int n = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &out->user, &out->nice, &out->system, &out->idle,
                    &out->iowait, &out->irq, &out->softirq, &out->steal);

    if (n >= 4) return 0;  // At least user, nice, system, idle must be read
    else return -1; 
}

static double compute_cpu_usage_pct(const cpu_stats *prev, const cpu_stats *cur) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long cur_idle  = cur->idle + cur->iowait;

    unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                    prev->idle + prev->iowait + prev->irq +
                                    prev->softirq + prev->steal;

    unsigned long long cur_total  = cur->user + cur->nice + cur->system +
                                    cur->idle + cur->iowait + cur->irq +
                                    cur->softirq + cur->steal;

    if (cur_total == prev_total) 
        return 0.0;
    else 
        return 100.0 * (1.0 - ((double)(cur_idle - prev_idle) / (double)(cur_total - prev_total)));
}


void *monitor_main(void *arg) {
    shared_state *state = (shared_state *)arg;

    FILE *log_file = fopen(LOG_FILENAME, "a");
    if (!log_file) {
        fprintf(stderr, "[monitor] Cannot open %s: %s\n",
                LOG_FILENAME, strerror(errno));
        return NULL;
    }

    // Initialize the log file if it's empty
    if (ftell(log_file) == 0) {
        fprintf(log_file,
            "Seconds,Nanoseconds,Commit_Count,Identity_Count,"
            "Account_Count,Info_Count,Buffer_Occupancy_Pct,CPU_Pct\n");
        fflush(log_file);
    }

    cpu_stats prev_stats;
    int have_prev = (read_cpu_stats(&prev_stats) == 0);

    // CLOCK_MONOTONIC for timing and CLOCK_REALTIME only for the timestamp (log entry) that is recorded in the log. 
    struct timespec next_wake;
    clock_gettime(CLOCK_MONOTONIC, &next_wake);

    while (global_running) {
        next_wake.tv_sec += 1;

        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wake, NULL);
        } while (rc == EINTR);
        
        if (!global_running) break;

        // 1) Capture and reset
        int commit, identity, account, info;
        counters_reset(state->counters, &commit, &identity, &account, &info);

        // 2) Buffer occupancy
        double occupancy = cb_occupancy_pct(state->buffer);

        // 3) CPU usage from /proc/stat
        double cpu_pct = 0.0;
        cpu_stats cur_stats;
        if (read_cpu_stats(&cur_stats) == 0) {
            if (have_prev) {
                cpu_pct = compute_cpu_usage_pct(&prev_stats, &cur_stats);
            }
            prev_stats = cur_stats;
            have_prev = 1;
        }

        // 4) Accurate timestamp of real time (wall-clock)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        // 5) Write CSV line + immediate flush for durability against crash
        fprintf(log_file, "%ld,%ld,%d,%d,%d,%d,%.4f,%.4f\n",
                (long)ts.tv_sec, (long)ts.tv_nsec,
                commit, identity, account, info,
                occupancy, cpu_pct);
        fflush(log_file);
    }

    fclose(log_file);
    return NULL;
}
