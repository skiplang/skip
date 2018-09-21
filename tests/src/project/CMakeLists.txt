# Set ${result} to the (non-recursive) directories contained in ${dir}
MACRO(SUBDIRLIST result dir)
  FILE(GLOB children RELATIVE ${dir} ${dir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${dir}/${child})
      LIST(APPEND dirlist ${dir}/${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

# Add a project system test.
#
# add_project_test(src)
#  src - The directory contaiing the skip.project.json file to test.
#
function(add_project_test src)
  if(NOT IS_DIRECTORY ${src})
    message(FATAL_ERROR "Test ${src} is not a directory")
  endif()

  set(RELATIVE ${CMAKE_CURRENT_SOURCE_DIR})

  if(NOT RUNNER)
    _findRunner(RUNNER ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  _skipPathToBinary(testok ${src} ".testok")
  _skipPathToBinary(testfail ${src} ".testfail")

  recursiveGlobSources(test_files ${src})

  set(depends skip_depends ${src} ${test_files})

  set(OPTIONS_FILE ${src}/options)
  if (EXISTS ${OPTIONS_FILE})
    file(READ ${OPTIONS_FILE} opts)
    string(STRIP ${opts} opts)
    separate_arguments(opts)
  else()
    set(opts "")
  endif()

  set(build_cmd ${ENV} ${RUNNER} --relative ${RELATIVE} ${src} --options ${opts})

  add_custom_command(
    OUTPUT ${testok}
    COMMAND rm -f ${testok}
    COMMAND touch ${testfail}
    COMMAND ${build_cmd}
    COMMAND rm ${testfail}
    DEPENDS ${depends}
    )

  mkparents(${testok})

  # relpath is dotted name of test file, relative to the root test directory.
  string(REPLACE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} relpath ${src})
  file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${relpath})
  string(REPLACE "/" "." relpath ${relpath})
  add_custom_target(test_project.${relpath} DEPENDS ${testok})

  add_dependencies(test_project test_project.${relpath})

endfunction()



add_custom_target(test_project)

SUBDIRLIST(invalid_tests ${CMAKE_CURRENT_SOURCE_DIR}/invalid)
SUBDIRLIST(valid_tests ${CMAKE_CURRENT_SOURCE_DIR}/valid)
set(all_tests ${valid_tests} ${invalid_tests})

foreach(f ${all_tests})
  add_project_test(${f})
endforeach()

add_dependencies(test test_project)
