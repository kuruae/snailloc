# snailloc

A learning-project custom thread-safe `malloc` implementation using mmap'd memory zones,
freelist-based reuse, and lock-free cross-thread free.  
Primarly built as a 42 Paris project then later extended with features that weren't allowed at school.

## What this is

A drop-in replacement for `malloc`/`free`/`realloc` that organises memory into
**zones** (tiny / small / large). It was built to understand how glibc's
allocator works under the hood: virtual memory, TLS, lock-free operations, and
freelist structures.

## Features

- **Zone-based allocation** — three size classes with dedicated mmap'd regions.
- **Freelist reuse** — freed tiny/small chunks are pushed onto an atomic
  per-zone freelist (O(1) pop/push). 
- **Lock-free cross-thread free** — `free()` in one thread atomically pushes
  the chunk onto the owning zone's freelist via CAS. No per-thread ownership
  checks needed.
- **Thread-local tiny/small zones** — each thread has its own TLS zone list,
  avoiding contention on the common path.
- **Global zone registry** — all zones are linked into a global list during
  creation, so `find_zone_by_addr` can validate pointers quickly — essential
  for `LD_PRELOAD` usage where `free()` may be called from an unknown thread
  or after the owning thread has exited.
- **Large allocation passthrough** — allocations above `SMALL_MAX` get their
  own mmap'd region, unmapped immediately on `free`.
- **Debug flags** — `MALLOC_CHECK_LEVEL` (misaligned/double-free detection),
  `MALLOC_FILL_ON_FREE` (zeroes freed data), `MALLOC_LOG_FREE`.

## How it works

```
Zone (tiny/small):
[zone_header][free_list][chunk_hdr][data][chunk_hdr][data]…[break]…
                                                  ↑___
freelist:  [chunk_hdr][chunk_hdr]NULL          (next stored in user data)
```

Three zone types:

| Type   | Max size                         | Allocation strategy                     |
|--------|----------------------------------|-----------------------------------------|
| TINY   | ≤ `page_size / 32` (~128 B)      | Shared zone, freelist + carve           |
| SMALL  | ≤ `page_size / 4` (~1024 B)      | Shared zone, freelist + carve           |
| LARGE  | anything larger                  | Private mmap, unmapped on free          |

### Freelist

Each tiny/small zone has an `_Atomic(t_chunk_header *) free_list` head.
- **`free()`** atomically CAS-es the chunk onto the head (stores the next
  pointer in the first 8 bytes of freed user data).
- **`malloc()`** atomically pops from the head. If the head chunk is too
  small for the request, it falls through to carve fresh space from
  `break_ptr` (the chunk stays in the freelist for a future match).
- Since the freelist operations are atomic on the zone's head, cross-thread
  free works without locks.

### Thread safety

- **TLS tiny/small zones** avoid contention on the common path.
- **Lock-free freelist** (CAS on `free_list` head) enables safe cross-thread
  free without mutexes for tiny/small.
- **Large allocations** use a mutex-protected global list (large allocs are
  rare in practice).
- The **global zone registry** is updated under a mutex during zone creation
  and is read without locks for `free()` pointer validation.

## Build

```bash
make              # builds libft_malloc_<HOSTTYPE>.so + libft_malloc.so symlink
make clean
make fclean
make re
```

### Use with LD_PRELOAD

```bash
export LD_LIBRARY_PATH=.
export LD_PRELOAD=./libft_malloc.so
./your_program
```

Or link directly:

```bash
gcc your_program.c -L. -lft_malloc
```

## Testing

```bash
make test TESTFILE=tests/test_tiny.c
make test TESTFILE=tests/test_small.c
# … etc
./tests/run_all_tests.sh
```

## Performance (vs glibc)

| Benchmark              | snailloc  | glibc     | Ratio  |
|------------------------|-----------|-----------|--------|
| tiny 100k alloc+free   | 79 ms     | 4.6 ms    | 17×    |
| mixed 50k ×5 rounds    | 18 347 ms | 40 ms     | 458×   |
| stress 256×200         | 1.5 ms    | 0.6 ms    | 2.4×   |
| 2 threads (50k each)   | 192 ms    | 8.4 ms    | 23×    |
| 8 threads (50k each)   | 7 245 ms  | 51 ms     | 141×   |

What can I say, these guys are good at this. In case you wondered this is the reason I name this project snailloc 🐌

## Some takeaways
The major flaw I have against glibc's malloc is pre caching of zones, they allocate a lot more memory than I do on startup which makes future allocations very easy   
as it often is, this is yet again a size vs speed constraint in a sort, so that's why I decided to stick with my implementation as more of a research project than a real alternative to malloc, in which case I woudld've made the same choice as they did.

The other part that really sticks out is how much I underestimated the issues thread-safety would provoke. The simplest approaches often involve serializing hot paths, which is something I tried to avoid in this design as much as I could.
