globSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)

foreach(f ${all_tests})
  add_skip_test(${f})
endforeach()
