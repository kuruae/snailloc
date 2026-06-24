#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NS_PER_SEC 1000000000L

static inline long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
}

// ─── Bench 1: single-threaded tiny alloc/fix ─────────────────────────
static long long bench_tiny(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return -1;

    long long start = now_ns();
    for (int i = 0; i < count; i++)
        ptrs[i] = malloc(64);
    long long mid = now_ns();
    for (int i = 0; i < count; i++)
        free(ptrs[i]);
    long long end = now_ns();

    free(ptrs);
    return (mid - start) + (end - mid);
}

// ─── Bench 2: single-threaded mixed sizes ────────────────────────────
static long long bench_mixed(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return -1;

    long long alloc_ns = 0, free_ns = 0;
    for (int r = 0; r < 5; r++) {
        long long t0 = now_ns();
        for (int i = 0; i < count; i++) {
            size_t sz = (i % 3 == 0) ? 32 : (i % 3 == 1) ? 256 : 2048;
            ptrs[i] = malloc(sz);
        }
        long long t1 = now_ns();
        for (int i = 0; i < count; i++)
            free(ptrs[i]);
        long long t2 = now_ns();
        alloc_ns += t1 - t0;
        free_ns += t2 - t1;
    }
    free(ptrs);
    return alloc_ns + free_ns;
}

// ─── Bench 3: single-threaded large allocs ───────────────────────────
static long long bench_large(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return -1;

    long long start = now_ns();
    for (int i = 0; i < count; i++)
        ptrs[i] = malloc(100000);
    long long mid = now_ns();
    for (int i = 0; i < count; i++)
        free(ptrs[i]);
    long long end = now_ns();

    free(ptrs);
    return (mid - start) + (end - mid);
}

// ─── Bench 4: multi-threaded ─────────────────────────────────────────
typedef struct {
    int count;
    long long total_ns;
} mt_args;

static void *mt_worker(void *arg) {
    mt_args *a = (mt_args *)arg;
    void **ptrs = malloc(a->count * sizeof(void *));
    if (!ptrs)
        return NULL;

    long long start = now_ns();
    for (int i = 0; i < a->count; i++)
        ptrs[i] = malloc(128);
    for (int i = 0; i < a->count; i++)
        free(ptrs[i]);
    a->total_ns = now_ns() - start;
    free(ptrs);
    return NULL;
}

#define MT_PER_THREAD 50000

static long long bench_mt(int nthreads) {
    int per_thread = MT_PER_THREAD;
    pthread_t *thr = malloc(nthreads * sizeof(pthread_t));
    mt_args *args = malloc(nthreads * sizeof(mt_args));
    if (!thr || !args) {
        free(thr);
        free(args);
        return -1;
    }

    for (int i = 0; i < nthreads; i++) {
        args[i].count = per_thread;
        pthread_create(&thr[i], NULL, mt_worker, &args[i]);
    }

    long long total = 0;
    for (int i = 0; i < nthreads; i++) {
        pthread_join(thr[i], NULL);
        total += args[i].total_ns;
    }
    free(thr);
    free(args);
    return total;
}

// ─── Bench 5: stress (alloc/free in a loop with reuse) ───────────────
static long long bench_stress(int count) {
    long long start = now_ns();
    for (int iter = 0; iter < 200; iter++) {
        void *ptrs[256];
        for (int i = 0; i < 256; i++)
            ptrs[i] = malloc((i % 5 + 1) * 16);
        for (int i = 255; i >= 0; i--)
            free(ptrs[i]);
    }
    return now_ns() - start;
}

// ─── runner ──────────────────────────────────────────────────────────
static void run(const char *name, long long (*fn)(int), int count, const char *unit,
                double divisor) {
    long long ns = fn(count);
    if (ns < 0) {
        printf("  %-22s  ERROR\n", name);
        return;
    }
    double val = ns / divisor;
    printf("  %-22s  %8.2f %s\n", name, val, unit);
}

int main(void) {
    printf("─── ft_malloc benchmark (vs glibc) ───\n");
    printf("  Run with:  LD_LIBRARY_PATH=. ./benchmark\n");
    printf("  Run w/o:   ./benchmark  (uses glibc)\n\n");

    printf("  Single-threaded:\n");
    run("tiny 100k alloc+free", bench_tiny, 100000, "ms", 1e6);
    run("mixed 50k ×5 rounds", bench_mixed, 50000, "ms", 1e6);
    run("large 1k alloc+free", bench_large, 1000, "ms", 1e6);
    run("stress 256×200", bench_stress, 0, "ms", 1e6);

    printf("\n  Multi-threaded (per-thread = 50k):\n");
    run("2 threads", bench_mt, 2, "ms", 1e6);
    run("4 threads", bench_mt, 4, "ms", 1e6);
    run("8 threads", bench_mt, 8, "ms", 1e6);

    printf("\n");
    return 0;
}
