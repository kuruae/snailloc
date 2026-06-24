#include "alloc.h"
#include "compiler_attrs.h"

__thread t_zones g_thread_zones = {NULL, NULL};
pthread_mutex_t g_large_mutex = PTHREAD_MUTEX_INITIALIZER;
t_zone_header *g_large_zones = NULL;

t_zone_header *create_zone(size_t size, t_zone_type type) {
	void *ptr = mmap(
		NULL,
		size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
	);

	if (UNLIKELY(ptr == MAP_FAILED))
		return NULL;

	t_zone_header *zone = (t_zone_header *)ptr;
	zone->next = NULL;
	zone->zone_size = size;
	zone->type = type;
	zone->break_ptr = (char *)zone + sizeof(t_zone_header);
	zone->owner = pthread_self();

	return zone;
}

void add_zone(t_zone_header *zone) {
	t_zone_header **curr = NULL;

	if (zone->type == ZONE_TINY)
		curr = &g_thread_zones.tiny;
	else if (zone->type == ZONE_SMALL)
		curr = &g_thread_zones.small;

	zone->next = *curr;
	*curr = zone;
}

void add_large_zone(t_zone_header *zone) {
	pthread_mutex_lock(&g_large_mutex);
	zone->next = g_large_zones;
	g_large_zones = zone;
	pthread_mutex_unlock(&g_large_mutex);
}

void remove_large_zone(t_zone_header *zone_to_remove) {
	pthread_mutex_lock(&g_large_mutex);
	t_zone_header **curr = &g_large_zones;
	while (*curr) {
		if (*curr == zone_to_remove) {
			*curr = zone_to_remove->next;
			zone_to_remove->next = NULL;
			break;
		}
		curr = &(*curr)->next;
	}
	pthread_mutex_unlock(&g_large_mutex);
}

int is_zone_in_local_list(t_zone_header *needle, t_zone_type type) {
	t_zone_header *head;
	if (type == ZONE_TINY)
		head = g_thread_zones.tiny;
	else if (type == ZONE_SMALL)
		head = g_thread_zones.small;
	else
		return 0;

	while (head) {
		if (head == needle)
			return 1;
		head = head->next;
	}
	return 0;
}
