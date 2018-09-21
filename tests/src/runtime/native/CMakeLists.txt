add_library(native.runtime.testutil
  "testutil.cpp"
  )
target_link_libraries(native.runtime.testutil skip_runtime.tests)
target_link_libraries(native.runtime.testutil gtest_main)

add_custom_target(test.native.runtime.tests)
add_dependencies(test test.native.runtime.tests)

globSources(cpp_tests ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
foreach(test ${cpp_tests})
  get_filename_component(test_we ${test} NAME_WE)
  set(target native.runtime.tests.${test_we})
  set(test_target test.${target})
  set(build_target build_native.${target})
  set(testok ${CMAKE_CURRENT_BINARY_DIR}/${test_we}.native.testok)
  set(testres ${CMAKE_CURRENT_BINARY_DIR}/${test_we}.native.res)
  set(testfail ${CMAKE_CURRENT_BINARY_DIR}/${test_we}.failing_native)

  add_executable(${build_target} EXCLUDE_FROM_ALL ${test})
  set_target_properties(${build_target} PROPERTIES OUTPUT_NAME ${test_we})
  target_link_libraries(${build_target} native.runtime.testutil skip_runtime.tests
    ${LIBEVENT_LIBRARY} ${Boost_LIBRARIES} ${DL_LIBRARIES} gtest_main)

  # This doesn't actually create the .failing_native file until we
  # start trying to run the test.  That means if it fails to compile
  # we won't get the signal - that will have to do.
  mkparents(${testok})
  add_custom_command(
    OUTPUT ${testok}
    COMMAND touch ${testfail}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test_we}
    COMMAND touch ${testok}
    COMMAND echo ${SIMPLE_PASSING_TESTRES} > ${testres}
    COMMAND rm -f ${testfail}
    DEPENDS ${build_target})

  add_custom_target(${test_target} DEPENDS ${testok})

  add_dependencies(test.native.runtime.tests ${test_target})
endforeach()
