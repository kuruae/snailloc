project(
  'snailloc',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',               
    'warning_level=3', # -Wall, -Wextra ...
    'werror=false',
    'buildtype=debug', 
  ],
)

inc = include_directories('include')

snailloc_sources = files(
  'src/allocator.cpp',
)

snailloc_lib = shared_library(
  'snailloc',
  snailloc_sources,
  include_directories: inc,
  install: true,
)

# Internal dependency object (makes linking tests/benchmarks easy for later)
snailloc_dep = declare_dependency(
  include_directories: inc,
  link_with: snailloc_lib,
)

# # Unit tests Setup
# test_basic = executable(
#   'test_basic',
#   'tests/test_basic.cpp',
#   dependencies: snailloc_dep,
# )
# test('Basic Alloc/Free Test', test_basic)
#
# test_threads = executable(
#   'test_threads',
#   'tests/test_threads.cpp',
#   dependencies: snailloc_dep,
# )
# test('Multi-Threaded Mailbox Test', test_threads)
#
# # Benchmark Target
# bench_exe = executable(
#   'bench_malloc',
#   'benchmarks/bench_malloc.cpp',
#   dependencies: snailloc_dep,
# )
# benchmark('Allocation Throughput Benchmark', bench_exe)
