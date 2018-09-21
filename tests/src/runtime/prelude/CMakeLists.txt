set(COMMON_ARGS
  RUNNER ${CMAKE_CURRENT_SOURCE_DIR}/run_test
  RELATIVE ${CMAKE_SOURCE_DIR})
recursiveGlobSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)
foreach(f ${all_tests})
  add_skip_test(${f} ${COMMON_ARGS})
endforeach()
