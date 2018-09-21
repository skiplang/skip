set(COMMON_ARGS
  RUNNER ${CMAKE_SOURCE_DIR}/prelude/testing/run_test
  RELATIVE ${CMAKE_SOURCE_DIR})
find_skip_sources(all_tests ${CMAKE_CURRENT_SOURCE_DIR})
foreach(f ${all_tests})
  add_skip_test(${f} ${COMMON_ARGS})
endforeach()
