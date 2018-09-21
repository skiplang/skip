find_program(NODE node HINTS /usr/local/bin)

mark_as_advanced(NODE)
if(NOT NODE)
  message(FATAL_ERROR "Node not found")
endif()
message(STATUS "Found node: ${NODE}")

# Check version
execute_process(COMMAND ${NODE} --version
  OUTPUT_VARIABLE NODE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)
# Node version comes out as vMAJOR.MINOR.PATCH
string(SUBSTRING "${NODE_VERSION}" 1 -1 NODE_VERSION)
if (NOT "${NODE_VERSION}" VERSION_GREATER "6")
  message(FATAL_ERROR "Node version ${NODE_VERSION} is bad.  Need 6 or higher.")
endif ()
set(ENV{NODE} ${NODE})
