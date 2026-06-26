#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline uint64_t start_cycles(void) {
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc\n\tlfence" : "=a"(lo), "=d"(hi)::"memory");
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t end_cycles(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtscp\n\tlfence" : "=a"(lo), "=d"(hi)::"%rcx", "memory");
    return ((uint64_t)hi << 32) | lo;
}

static void pin_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

static const char *allocator_name(void) {
    Dl_info info;
    if (dladdr((const void *)&malloc, &info) && info.dli_fname)
        return info.dli_fname;
    return "unknown";
}

#define WARMUP 3
#define TRIALS 7

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static uint64_t bench_tiny(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return 0;

    uint64_t start = start_cycles();
    for (int i = 0; i < count; i++)
        ptrs[i] = malloc(64);
    for (int i = 0; i < count; i++)
        free(ptrs[i]);
    uint64_t end = end_cycles();

    free(ptrs);
    return end - start;
}

static uint64_t bench_small(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return 0;

    uint64_t start = start_cycles();
    for (int i = 0; i < count; i++)
        ptrs[i] = malloc(512);
    for (int i = 0; i < count; i++)
        free(ptrs[i]);
    uint64_t end = end_cycles();

    free(ptrs);
    return end - start;
}

static uint64_t bench_mixed(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return 0;

    uint64_t total = 0;
    for (int r = 0; r < 5; r++) {
        uint64_t t0 = start_cycles();
        for (int i = 0; i < count; i++) {
            size_t sz;
            switch (i % 4) {
            case 0:
                sz = 16;
                break;
            case 1:
                sz = 64;
                break;
            case 2:
                sz = 256;
                break;
            case 3:
                sz = 1024;
                break;
            default:
                sz = 64;
            }
            ptrs[i] = malloc(sz);
        }
        uint64_t t1 = end_cycles();
        for (int i = 0; i < count; i++)
            free(ptrs[i]);
        total += t1 - t0;
    }
    free(ptrs);
    return total;
}

#define LARGE_SZ 65536

static uint64_t bench_large(int count) {
    void **ptrs = malloc(count * sizeof(void *));
    if (!ptrs)
        return 0;

    uint64_t start = start_cycles();
    for (int i = 0; i < count; i++)
        ptrs[i] = malloc(LARGE_SZ);
    uint64_t mid = end_cycles();
    for (int i = 0; i < count; i++)
        free(ptrs[i]);
    uint64_t end = end_cycles();

    free(ptrs);
    return (mid - start) + (end - mid);
}

static uint64_t bench_stress(int iterations) {
    uint64_t start = start_cycles();
    for (int it = 0; it < iterations; it++) {
        void *ptrs[256];
        for (int i = 0; i < 256; i++)
            ptrs[i] = malloc(((i % 5) + 1) * 16);
        for (int i = 255; i >= 0; i--)
            free(ptrs[i]);
    }
    return end_cycles() - start;
}

static uint64_t bench_realloc(int count) {
    void *p = malloc(16);
    if (!p)
        return 0;

    uint64_t start = start_cycles();
    for (int i = 0; i < count; i++) {
        p = realloc(p, (i % 7 + 1) * 32);
        if (!p)
            return 0;
    }
    uint64_t end = end_cycles();
    free(p);
    return end - start;
}

typedef struct {
    int count;
    int cpu;
    uint64_t cycles;
} mt_args;

static void *mt_worker(void *arg) {
    mt_args *a = (mt_args *)arg;
    if (a->cpu >= 0)
        pin_cpu(a->cpu);

    void **ptrs = malloc(a->count * sizeof(void *));
    if (!ptrs)
        return NULL;

    uint64_t start = start_cycles();
    for (int i = 0; i < a->count; i++)
        ptrs[i] = malloc(128);
    for (int i = 0; i < a->count; i++)
        free(ptrs[i]);
    a->cycles = end_cycles() - start;

    free(ptrs);
    return NULL;
}

#define MT_ALLOCS 50000

static uint64_t bench_mt(int nthreads) {
    pthread_t *thr = malloc(nthreads * sizeof(pthread_t));
    mt_args *args = malloc(nthreads * sizeof(mt_args));
    if (!thr || !args) {
        free(thr);
        free(args);
        return 0;
    }

    for (int i = 0; i < nthreads; i++) {
        args[i].count = MT_ALLOCS;
        args[i].cpu = i;
        if (pthread_create(&thr[i], NULL, mt_worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            free(thr);
            free(args);
            return 0;
        }
    }

    uint64_t total = 0;
    for (int i = 0; i < nthreads; i++) {
        pthread_join(thr[i], NULL);
        total += args[i].cycles;
    }
    free(thr);
    free(args);
    return total;
}

typedef uint64_t (*bench_fn)(int);

static void run_bench(const char *name, bench_fn fn, int arg, int divisor, const char *unit) {
    for (int i = 0; i < WARMUP; i++)
        fn(arg);

    uint64_t samples[TRIALS];
    for (int i = 0; i < TRIALS; i++)
        samples[i] = fn(arg);

    qsort(samples, TRIALS, sizeof(samples[0]), cmp_u64);

    double val = (double)samples[TRIALS / 2] / divisor;
    printf("  %-22s  %8.0f  %s\n", name, val, unit);
}

int main(void) {
    printf("  malloc from: %s\n\n", allocator_name());

    pin_cpu(0);
    printf("  Single-threaded (cycles per operation):\n");

    run_bench("tiny 100k alloc+free", bench_tiny, 100000, 200000, "cyc/op");
    run_bench("small 50k alloc+free", bench_small, 50000, 100000, "cyc/op");
    run_bench("large 1k alloc+free", bench_large, 1000, 2000, "cyc/op");
    run_bench("mixed 50k ×5 rounds", bench_mixed, 50000, 250000, "cyc/op");
    run_bench("stress 256×200", bench_stress, 200, 51200, "cyc/op");
    run_bench("realloc 10k", bench_realloc, 10000, 10000, "cyc/op");

    printf("\n  Multi-threaded (cycles per alloc+free, per-thread 50k):\n");
    run_bench("2 threads", (bench_fn)bench_mt, 2, 200000, "cyc/op");
    run_bench("4 threads", (bench_fn)bench_mt, 4, 400000, "cyc/op");
    run_bench("8 threads", (bench_fn)bench_mt, 8, 800000, "cyc/op");

    printf("\n");
    return 0;
}
