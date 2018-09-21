find_package(Boost 1.53 REQUIRED COMPONENTS thread system context filesystem)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
    CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
    CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(Boost_LIBRARY_DIR ${Boost_LIBRARY_DIR_RELEASE})
else()
  set(Boost_LIBRARY_DIR ${Boost_LIBRARY_DIR_DEBUG})
endif()
set(Boost_LIBRARIES ${Boost_LIBRARIES} CACHE STRING "")
message("Found Boost: ${Boost_INCLUDE_DIR} ${Boost_LIBRARY_DIR}")

add_library(boost INTERFACE)
target_include_directories(boost INTERFACE ${Boost_INCLUDE_DIR})
target_link_libraries(boost INTERFACE ${Boost_LIBRARIES} -Wl,-rpath,${Boost_LIBRARY_DIR})
target_compile_options(boost INTERFACE)
