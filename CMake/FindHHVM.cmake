find_program(HHVM hhvm)

if(NOT HHVM)
  message(STATUS "Could not find a valid HHVM - disabling PHP tests.")
  return()
endif()

mark_as_advanced(HHVM)

message(STATUS "Found HHVM: ${HHVM}")
