# Add a test which translates a sk file to Skip.
#
# add_serialization_generator_test(src)
#
function(add_serialization_generator_test src)
  if(NOT src MATCHES ".sk$")
    message(FATAL_ERROR "Test ${src} doesn't end with .sk")
  endif()
  string(REGEX REPLACE ".sk$" "" path_we ${src})

  set(RELATIVE ${CMAKE_CURRENT_SOURCE_DIR})

  if(NOT RUNNER)
    _findRunner(RUNNER ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  # relpath is dotted name of test file, relative to the root test directory.
  string(REPLACE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} relpath ${path_we})
  file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${relpath})
  string(REPLACE "/" "." relpath ${relpath})

  _skipPathToBinary(testok ${src} ".testok")
  _skipPathToBinary(testfail ${src} ".testfail")

  _skipPathToSource(srcPath_we ${path_we} "")

  set(depends skip_serialization_generator ${src} ${srcPath_we}.exp)

  set(build_cmd ${ENV} ${RUNNER} --relative ${RELATIVE} ${src})

  add_custom_command(
    OUTPUT ${testok}
    COMMAND rm -f ${testok}
    COMMAND touch ${testfail}
    COMMAND ${build_cmd}
    COMMAND rm ${testfail}
    DEPENDS ${depends}
    )

  mkparents(${testok})
  add_custom_target(test_serialization_generator.${relpath} DEPENDS ${testok})
  add_dependencies(test_serialization_generator test_serialization_generator.${relpath})


endfunction()

add_custom_target(test_serialization_generator)

recursiveGlobSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)
list(REMOVE_ITEM all_tests ${CMAKE_CURRENT_SOURCE_DIR}/TestHelpers.sk)

foreach(f ${all_tests})
  add_serialization_generator_test(${f})
endforeach()

add_dependencies(test test_serialization_generator)
