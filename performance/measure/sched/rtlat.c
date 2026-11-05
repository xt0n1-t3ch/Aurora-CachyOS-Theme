// rtlat SECONDS: a SCHED_RR priority-1 thread (the policy ananicy-cpp gives kwin_wayland) wakes every millisecond
// on an absolute timer and records how late it ran; prints p50, p99, p99.9 and max lateness in microseconds.
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int cmp(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return (x > y) - (x < y); }

// SECONDS defaults to 10. Exits 1 when the real-time policy cannot be set, which needs CAP_SYS_NICE or an
// RLIMIT_RTPRIO allowance.
int main(int argc, char **argv)
{
    const int seconds = argc > 1 ? atoi(argv[1]) : 10, n = seconds * 1000;
    struct sched_param param = {.sched_priority = 1};
    if (sched_setscheduler(0, SCHED_RR, &param) != 0) { perror("sched_setscheduler"); return 1; }
    long *late = malloc(sizeof(long) * n);
    struct timespec next, now;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < n; ++i) {
        next.tv_nsec += 1000000;
        if (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        clock_gettime(CLOCK_MONOTONIC, &now);
        late[i] = ((now.tv_sec - next.tv_sec) * 1000000000L + (now.tv_nsec - next.tv_nsec)) / 1000;
    }
    qsort(late, n, sizeof(long), cmp);
    printf("{\"p50\": %ld, \"p99\": %ld, \"p999\": %ld, \"max\": %ld}\n", late[n / 2], late[n * 99 / 100], late[n * 999 / 1000], late[n - 1]);
    return 0;
}
