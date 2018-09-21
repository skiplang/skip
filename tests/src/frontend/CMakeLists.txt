recursiveGlobSources(all_tests ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)

foreach(f ${all_tests})
  add_skip_test(${f})
endforeach()

# --------------------------------------------------

function(add_inttests op)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime/inttests-${op}.sk ${CMAKE_CURRENT_BINARY_DIR}/runtime/inttests-${op}.exp
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gen_inttests --op ${op} --output-dir ${CMAKE_CURRENT_BINARY_DIR}/runtime
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gen_inttests
  )
add_skip_test(${CMAKE_CURRENT_BINARY_DIR}/runtime/inttests-${op}.sk)
endfunction()

# Ensure that if gen_inttests changes we re-run cmake
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gen_inttests)

execute_process(
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gen_inttests --list-ops
  OUTPUT_VARIABLE inttests_ops
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

# TODO: Reenable these
#foreach(op ${inttests_ops})
#  add_inttests(${op})
#endforeach()
