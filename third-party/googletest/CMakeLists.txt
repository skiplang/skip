if(NOT DISABLE_TP_GTEST)
  add_library(gtest_main STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/src/gtest-all.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/src/gtest_main.cc"
    )
  target_include_directories(gtest_main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest")
  target_include_directories(gtest_main PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/googletest/include")
endif()
