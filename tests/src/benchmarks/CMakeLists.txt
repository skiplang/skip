# We don't want any of the benchmarks to show up in the
# test_native targets
# Set FILTER to a hopefully garbage name to make that happen

set(FILTER "z/z/z/z/z")

globSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)

foreach(f ${all_tests})
  add_skip_test(${f})
endforeach()

add_dependencies(test_benchmark
  test_native.tests.src.benchmarks
)
