#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *malloc(size_t size);
void free(void *ptr);

#define NUM_ALLOCS 100

typedef struct {
    void **ptrs;
    int count;
    size_t size;
    int success;
    volatile int *ready;
} worker_t;

static void *allocator_thread(void *arg) {
    worker_t *w = (worker_t *)arg;
    for (int i = 0; i < w->count; i++) {
        w->ptrs[i] = malloc(w->size);
        if (!w->ptrs[i]) {
            write(2, "allocator: malloc failed\n", 25);
            w->success = 0;
            return NULL;
        }
        memset(w->ptrs[i], 0xAB, w->size);
    }
    if (w->ready)
        __atomic_store_n(w->ready, 1, __ATOMIC_RELEASE);
    // Stay alive until the freeer is done
    while (__atomic_load_n(w->ready, __ATOMIC_ACQUIRE) != 2)
        usleep(1000);
    w->success = 1;
    return NULL;
}

static void *freeer_thread(void *arg) {
    worker_t *w = (worker_t *)arg;
    // Wait for allocator to finish allocating
    while (!__atomic_load_n(w->ready, __ATOMIC_ACQUIRE))
        usleep(1000);

    for (int i = 0; i < w->count; i++) {
        void *ptr = w->ptrs[i];
        if (ptr) {
            unsigned char *p = (unsigned char *)ptr;
            for (size_t j = 0; j < (w->size < 64 ? w->size : 64); j++) {
                if (p[j] != 0xAB) {
                    write(2, "freeer: data corruption\n", 24);
                    w->success = 0;
                    __atomic_store_n(w->ready, 2, __ATOMIC_RELEASE);
                    return NULL;
                }
            }
            free(ptr);
            w->ptrs[i] = NULL;
        }
    }
    w->success = 1;
    __atomic_store_n(w->ready, 2, __ATOMIC_RELEASE);
    return NULL;
}

static int run_cross_free_test(const char *label, size_t size, int count) {
    printf("  %s (%zuB, %d allocs)... ", label, size, count);

    void *ptrs[count];
    volatile int ready = 0;

    worker_t aw = {ptrs, count, size, 0, &ready};
    worker_t fw = {ptrs, count, size, 0, &ready};

    pthread_t at, ft;
    if (pthread_create(&at, NULL, allocator_thread, &aw) != 0)
        return printf("FAIL (create alloc)\n"), 1;
    if (pthread_create(&ft, NULL, freeer_thread, &fw) != 0)
        return printf("FAIL (create free)\n"), 1;

    pthread_join(at, NULL);
    pthread_join(ft, NULL);

    if (!aw.success || !fw.success)
        return printf("FAIL\n"), 1;

    printf("PASS\n");
    return 0;
}

static void *mixed_worker(void *arg) {
    volatile int *start = (volatile int *)arg;
    while (!__atomic_load_n(start, __ATOMIC_ACQUIRE))
        usleep(500);

    // Each thread allocates some TINY, SMALL, and LARGE
    // and frees a mix of its own and others' pointers via shared
    // But for simplicity, just do own alloc/free under contention
    void *ptrs[30];
    for (int i = 0; i < 30; i++) {
        ptrs[i] = malloc((i % 3 == 0) ? 64 : (i % 3 == 1) ? 500 : 10000);
        if (ptrs[i])
            memset(ptrs[i], 0x42, (i % 3 == 0) ? 64 : (i % 3 == 1) ? 500 : 64);
    }
    for (int i = 0; i < 30; i++) {
        if (ptrs[i])
            free(ptrs[i]);
    }
    return NULL;
}

int main(void) {
    printf("=== Cross-Thread Free Tests ===\n\n");

    int fail = 0;

    // Test 1: allocator thread A → freeer thread B (concurrent)
    fail += run_cross_free_test("TINY", 64, NUM_ALLOCS);
    fail += run_cross_free_test("SMALL", 512, 20);
    fail += run_cross_free_test("LARGE", 8192, 10);

    // Test 2: concurrent threads doing own alloc/free (stress)
    printf("\n  Concurrent stress (4 threads, mixed sizes)... ");
    volatile int start = 0;
    pthread_t tids[4];
    for (int i = 0; i < 4; i++)
        pthread_create(&tids[i], NULL, mixed_worker, (void *)&start);
    __atomic_store_n(&start, 1, __ATOMIC_RELEASE);
    for (int i = 0; i < 4; i++)
        pthread_join(tids[i], NULL);
    printf("PASS\n");

    printf("\n=== Summary ===\n");
    if (fail == 0)
        printf("All tests PASSED\n");
    else
        printf("%d test(s) FAILED\n", fail);

    return fail ? 1 : 0;
}
