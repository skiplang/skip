# Find LibEvent
#   LIBEVENT_FOUND
#   LIBEVENT_INCLUDE_DIR
#   LIBEVENT_LIBRARY_DIR
#   LIBEVENT_LIBRARY

if(DEFINED LIBEVENT_FOUND)
  message(STATUS "Cached libevent: ${LIBEVENT_INCLUDE_DIR} ${LIBEVENT_LIBRARY}")
  return()
endif()

set(LIBEVENT_EXTRA_PREFIXES /usr/local /opt/local)
foreach(prefix ${LIBEVENT_EXTRA_PREFIXES})
  list(APPEND LIBEVENT_INCLUDE_PATHS "${prefix}/include")
  list(APPEND LIBEVENT_LIB_PATHS "${prefix}/lib")
endforeach()

find_path(LIBEVENT_INCLUDE_DIR event.h PATHS ${LIBEVENT_INCLUDE_PATHS})
find_library(LIBEVENT_LIBRARY NAMES event PATHS ${LIBEVENT_LIB_PATHS})

if(LIBEVENT_LIBRARY AND LIBEVENT_INCLUDE_DIR)
  set(LIBEVENT_FOUND TRUE CACHE BOOL "")
  message(STATUS "Found libevent: ${LIBEVENT_INCLUDE_DIR} ${LIBEVENT_LIBRARY}")
endif()

get_filename_component(LIBEVENT_LIBRARY_DIR ${LIBEVENT_LIBRARY} DIRECTORY CACHE)

if(NOT LIBEVENT_FOUND)
  if(LibEvent_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find libevent")
  else()
    message(STATUS "Could not find libevent")
  endif()
  return()
endif()
