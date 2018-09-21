globSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)

foreach(f ${all_tests})
  add_skip_test(${f})
endforeach()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testok
  COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testok
  COMMAND touch ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testfail
  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/test_reach.py
  COMMAND mv ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testfail ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testok
  COMMAND echo ${SIMPLE_PASSING_TESTRES} > ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.res
  DEPENDS skip_to_native native_cc.tests ${CMAKE_CURRENT_SOURCE_DIR}/test_reach.py
  )

add_custom_target(test_native.tests.src.native.test_reach DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_reach.native.testok)
add_dependencies(test_native test_native.tests.src.native.test_reach)
