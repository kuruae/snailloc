#ifndef SIZE_UTILS_H
#define SIZE_UTILS_H

#include "compiler_attrs.h"
#include <stddef.h>
#include <unistd.h>

#define TINY_RATIO 32
#define SMALL_RATIO 4
#define MIN_ALLOC_COUNT 100

/* Eagerly-initialised globals — avoids cross-TU function-call overhead
 * on the hot path (every malloc() calls get_tiny_max / get_small_max).   */
extern size_t g_system_page_size;
extern size_t g_tiny_max;
extern size_t g_small_max;

static inline size_t get_system_page_size(void) { return g_system_page_size; }
static inline size_t get_tiny_max(void)         { return g_tiny_max; }
static inline size_t get_small_max(void)        { return g_small_max; }

size_t get_zone_size(size_t max_type_size);

#endif
