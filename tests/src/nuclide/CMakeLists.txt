
# ---------------------------------------------------------------------------
# Test Nuclide Outline View Integration

set(OUTLINE_TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/outline-test.sk)
set(OUTLINE_EXP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/outline-test.exp)
set(OUTLINE_BIN ${SKIP_BIN_PATH}/skip_outline)
set(OUTLINE_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/outline-test.out)

_skipPathToBinary(outline_testok ${OUTLINE_TEST_FILE} ".outline-test.testok")
_skipPathToBinary(outline_testfail ${OUTLINE_TEST_FILE} ".outline-test.testfail")
_skipPathToBinary(outline_testres ${OUTLINE_TEST_FILE} ".outline-test.res")

set(generate_output ${OUTLINE_BIN} ${OUTLINE_TEST_FILE} > ${OUTLINE_OUTPUT_FILE})

add_custom_command(
  OUTPUT ${outline_testok}
  COMMAND rm -f ${outline_testok}
  COMMAND touch ${outline_testfail}
  COMMAND ${generate_output}
  COMMAND diff ${OUTLINE_EXP_FILE} ${OUTLINE_OUTPUT_FILE}
  COMMAND rm ${outline_testfail}
  COMMAND touch ${outline_testok}
  COMMAND echo ${SIMPLE_PASSING_TESTRES} > ${outline_testres}
  DEPENDS ${OUTLINE_BIN} ${OUTLINE_TEST_FILE} ${OUTLINE_EXP_FILE}
  )

mkparents(${outline_testok})
add_custom_target(test_nuclide_outline DEPENDS ${outline_testok})

add_custom_target(
  update_outline_baseline
  COMMAND ${generate_output}
  COMMAND cp ${OUTLINE_OUTPUT_FILE} ${OUTLINE_EXP_FILE}
  DEPENDS ${OUTLINE_BIN} ${OUTLINE_JS_FILE} ${OUTLINE_TEST_FILE})

# ---------------------------------------------------------------------------
# Test Nuclide skip_check integration, as well as mini-prelude.

set(CHECK_TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/mini-prelude/mini.sk)
set(CHECK_EXP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/check-test.exp)
set(CHECK_BIN ${SKIP_BIN_PATH}/skip_check)
set(CHECK_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/check-test.out)

_skipPathToBinary(check_testok ${CHECK_TEST_FILE} ".check-test.testok")
_skipPathToBinary(check_testfail ${CHECK_TEST_FILE} ".check-test.testfail")
_skipPathToBinary(check_testres ${CHECK_TEST_FILE} ".check-test.res")

set(generate_output ${CHECK_BIN} ${CHECK_TEST_FILE} > ${CHECK_OUTPUT_FILE})

add_custom_command(
  OUTPUT ${check_testok}
  COMMAND rm -f ${check_testok}
  COMMAND touch ${check_testfail}
  COMMAND ${generate_output}
  COMMAND diff ${CHECK_EXP_FILE} ${CHECK_OUTPUT_FILE}
  COMMAND rm ${check_testfail}
  COMMAND touch ${check_testok}
  COMMAND echo ${SIMPLE_PASSING_TESTRES} > ${check_testres}
  DEPENDS ${CHECK_BIN} ${CHECK_TEST_FILE} ${CHECK_EXP_FILE}
  )

mkparents(${check_testok})
add_custom_target(test_nuclide_check DEPENDS ${check_testok})

add_custom_target(
  update_check_baseline
  COMMAND ${generate_output}
  COMMAND cp ${CHECK_OUTPUT_FILE} ${CHECK_EXP_FILE}
  DEPENDS ${CHECK_JS_FILE} ${CHECK_TEST_FILE})


# ---------------------------------------------------------------------------
# Tests for Nuclide LSP Integration


# Add a skip test which:
#   - copy the tests/src/nuclide/lsp dir to a temporary test location
#   - starts a skip_lsp process rooted at the test dir
#   - executes a test script against that skip_lsp process, sending messsages
#     and verifying responses
#
# add_lsp_test(src)
#
function(add_lsp_test src)

  if(NOT src MATCHES ".script.json$")
    message(FATAL_ERROR "Test ${src} doesn't end with .script.json")
  endif()
  string(REGEX REPLACE ".script.json$" "" path_we ${src})

  string(REPLACE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} relpath ${path_we})
  file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${relpath})
  string(REPLACE "/" "." relpath ${relpath})

  _skipPathToBinary(testok ${src} ".testok")
  _skipPathToBinary(testfail ${src} ".testfail")

  set(
    build_cmd
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_skip_lsp_test
    ${src}
    ${CMAKE_CURRENT_SOURCE_DIR}/lsp/src
    ${SKIP_BIN_PATH}/skip_lsp
  )

  add_custom_command(
    OUTPUT ${testok}
    COMMAND rm -f ${testok}
    COMMAND touch ${testfail}
    COMMAND ${build_cmd}
    COMMAND rm ${testfail}
    DEPENDS skip_lsp ${src}
    )

  mkparents(${testok})
  add_custom_target(test_skip_lsp.${relpath} DEPENDS ${testok})

  add_dependencies(test_skip_lsp test_skip_lsp.${relpath})
endfunction()

recursiveGlobSources(lsp_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.script.json)


add_custom_target(test_skip_lsp)

foreach(f ${lsp_tests})
  add_lsp_test(${f})
endforeach()


# ---------------------------------------------------------------------------
# Tests for Nuclide Integration

add_custom_target(
  test_nuclide
  DEPENDS
    test_nuclide_outline
    test_nuclide_check
    # TODO: Reenable once reactive cache hang is fixed
    # test_skip_lsp
  )

add_dependencies(test test_nuclide)
