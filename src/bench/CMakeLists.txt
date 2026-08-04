# Benchmarks are built only when asked for (`./project.py --bench`), never by `--build`.
#
# `EXCLUDE_FROM_ALL` is what keeps them off every CI leg's build: the legs run `--cmake --build`,
# and the linux-docker image runs `--all` (Dockerfile), so a benchmark in the default target would
# be compiled on all of them for nothing. Note that this does NOT keep them out of
# `compile_commands.json` -- clang-tidy still sees them, and its zero-warning gate still applies.
#
# They also live outside `test/` on purpose. That directory globs every `.cpp` into a GoogleTest
# target registered with `gtest_discover_tests`, which runs the binary at build time; and the
# release checklist runs every binary under `build/test/` and reconciles the count against the
# `TEST(` macros, which a benchmark would quietly throw off.

macro(CELESTIAL_ADD_BENCH bench_path bench_name)
  message("Adding benchmark ${bench_name}")
  add_executable(${bench_name} EXCLUDE_FROM_ALL ${bench_path})
  target_include_directories(${bench_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(${bench_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bench
  )
  add_dependencies(benchmarks ${bench_name})
endmacro()

# One target to build them all: `cmake --build <dir> --target benchmarks`.
add_custom_target(benchmarks)

file(GLOB_RECURSE BENCH_PATHS "bench_*.cpp")

foreach(bench_path IN LISTS BENCH_PATHS)
  get_filename_component(bench_name ${bench_path} NAME_WE)
  message("Found benchmark: ${bench_path}")
  CELESTIAL_ADD_BENCH(${bench_path} ${bench_name})
endforeach()
