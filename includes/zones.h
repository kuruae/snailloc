#ifndef ZONES_H
#define ZONES_H

#include "alloc.h"
#include "compiler_attrs.h"
#include <stddef.h>

t_zone_header   *create_zone(size_t size, t_zone_type type);
void            add_zone(t_zone_header *zone) ATTR_NONNULL(1);
void            add_large_zone(t_zone_header *zone) ATTR_NONNULL(1);
void            remove_large_zone(t_zone_header *zone) ATTR_NONNULL(1);
int             is_zone_in_local_list(t_zone_header *zone, t_zone_type type);

#endif
