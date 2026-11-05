// burst: how long a short burst of work takes right after the CPU has been idle, the moment a click or a key
// press meets an idle core. Sleeps, then times a fixed integer loop; prints the median and the 90th percentile in
// microseconds over N samples.  burst [N] [SLEEP_MS]  (defaults: N 20, SLEEP_MS 200)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static int cmp(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    const int n = argc > 1 ? atoi(argv[1]) : 20;
    const long sleep_ms = argc > 2 ? atol(argv[2]) : 200;
    double *samples = calloc((size_t)n, sizeof *samples);
    volatile uint64_t sink = 0;
    for (int i = 0; i < n; ++i) {
        const struct timespec pause = {sleep_ms / 1000, (sleep_ms % 1000) * 1000000L};
        nanosleep(&pause, NULL);
        const double start = now_us();
        uint64_t x = 88172645463325252ull;
        for (int k = 0; k < 4000000; ++k) { // about 2 ms at full clock on a Zen 3 core
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
        }
        sink += x;
        samples[i] = now_us() - start;
    }
    qsort(samples, (size_t)n, sizeof *samples, cmp);
    printf("%.0f %.0f\n", samples[n / 2], samples[(n * 9) / 10]);
    free(samples);
    // Always 0; reading sink keeps the compiler from removing the loop.
    return (int)(sink & 0);
}
