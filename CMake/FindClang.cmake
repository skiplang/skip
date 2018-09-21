#
# Check for Clang
#
# Defines the following:
# CLANG_FOUND
# CLANG_EXECUTABLE

if(CLANG_FOUND)
  return()
endif()
set(CLANG_FOUND FALSE CACHE BOOL "TRUE if CLANG was found")
mark_as_advanced(CLANG_FOUND)

find_program(CLANG_EXECUTABLE "clang++" HINTS ${HINT} /usr/lib/llvm-5.0/bin)
if (NOT CLANG_EXECUTABLE)
  message(FATAL_ERROR "clang++ is required")
endif()

execute_process(
  COMMAND ${CLANG_EXECUTABLE} --version
  OUTPUT_VARIABLE CLANG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
string(REGEX REPLACE
  ".* version ([0-9]+\\.[0-9]+\\.[0-9]+)[ -].*"
  "\\1"
  CLANG_VERSION ${CLANG_VERSION})
if(CLANG_VERSION VERSION_LESS "5.0.0")
  message(FATAL_ERROR "clang version ${CLANG_VERSION} is incorrect. Need at least version 5.0.0. Please install a newer version.")
endif()

set(CLANG_FOUND TRUE CACHE BOOL "TRUE if CLANG was found" FORCE)
