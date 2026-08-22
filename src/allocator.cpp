#include "constants.hpp"
#include "snailloc.hpp"
#include <sys/mman.h>

namespace snail {

void *snailloc(size_t size) {
    void *ptr{nullptr};
    if (size > REGULAR_ALLOC_MAX)
        return ptr; // TODO
    return ptr;
}

} // namespace snail

// void *malloc(size_t size) {}
