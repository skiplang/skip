if(NOT DEFINED GTEST_FOUND)
  find_path(GTEST_INCLUDE NAMES gtest/gtest.h HINTS ${GTEST_HINT}/include)
  find_path(GTEST_SRC NAMES src/gtest-all.cc HINTS ${GTEST_HINT} PATH_SUFFIXES src/googletest)
  find_library(GTEST_LIBRARY NAMES libgtest.a)
  if(GTEST_INCLUDE AND GTEST_LIBRARY AND GTEST_SRC)
    set(GTEST_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(GTEST_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

if(GTEST_FOUND)
  message("Using built-in gtest: ${GTEST_INCLUDE} ${GTEST_LIBRARY} ${GTEST_SRC}")

  add_library(gtest_main STATIC
    "${GTEST_SRC}/src/gtest-all.cc"
    "${GTEST_SRC}/src/gtest_main.cc"
    )
  target_include_directories(gtest_main PRIVATE ${GTEST_SRC})
  target_include_directories(gtest_main PUBLIC ${GTEST_INCLUDE})

  target_link_libraries(gtest_main INTERFACE ${GFLAGS_LIBRARY})

  return()
endif()

if(DISABLE_TP_GTEST)
  message(FATAL_ERROR "gtest not found and third-party disabled")
endif()

message("Using third-party gtest")

add_library(gtest_main STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/src/gtest-all.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/src/gtest_main.cc"
  )
target_include_directories(gtest_main PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest")
target_include_directories(gtest_main PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/include")
