#include "alloc.h"
#include "zones.h"

static void free_own_large_zones(void) {
    pthread_t self = pthread_self();

    pthread_mutex_lock(&g_large_mutex);
    t_zone_header **curr = &g_large_zones;
    while (*curr) {
        t_zone_header *zone = *curr;
        if (pthread_equal(zone->owner, self)) {
            *curr = zone->next;
            munmap(zone, zone->zone_size);
        } else {
            curr = &zone->next;
        }
    }
    pthread_mutex_unlock(&g_large_mutex);
}

static void cleanup_thread_zones() {
    // TINY/SMALL zones are in the global registry (g_all_zones).
    // They're left mapped — OS reclaims on process exit.
    // LARGE zones owned by this thread are cleaned up via the mutex-protected
    // list.
    free_own_large_zones();
}

static pthread_key_t cleanup_key;
static pthread_once_t cleanup_once = PTHREAD_ONCE_INIT;

static void init_cleanup_key() { pthread_key_create(&cleanup_key, cleanup_thread_zones); }

void register_thread_cleanup() {
    pthread_once(&cleanup_once, init_cleanup_key);
    pthread_setspecific(cleanup_key, (void *)1);
}
