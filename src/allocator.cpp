#include "const.hpp"
#include "snailloc.hpp"
#include <sys/mman.h>

void *snailloc(size_t size) {
    void *ptr{nullptr};
    if (size > REGULAR_ALLOC_MAX)
        return ptr; // TODO
    return ptr;
}
