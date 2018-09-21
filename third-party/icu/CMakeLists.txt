if(NOT DEFINED ICU4C_FOUND)
  # Look for an existing icu4c
  set(ICU_PATH /usr/local/opt/icu4c)
  find_library(LIBICU_UC NAMES libicuuc.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_I18N NAMES libicui18n.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_UC NAMES libicuuc.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_I18N NAMES libicui18n.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_IO NAMES libicuio.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_TU NAMES libicutu.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_library(LIBICU_DATA NAMES libicudata.a PATHS ${ICU_PATH}/lib NO_DEFAULT_PATH)
  find_path(LIBICU_INCLUDE unicode/utf8.h PATHS ${ICU_PATH}/include NO_DEFAULT_PATH)
  if(LIBICU_UC AND LIBICU_I18N AND LIBICU_UC AND LIBICU_I18N AND LIBICU_IO AND LIBICU_TU AND LIBICU_DATA AND LIBICU_INCLUDE)
    set(ICU4C_FOUND YES CACHE BOOL "" FORCE)
  else()
    set(ICU4C_FOUND NO CACHE BOOL "" FORCE)
  endif()
endif()

if(ICU4C_FOUND)
  message("Using built-in icu4c: ${LIBICU_INCLUDE} (${LIBICU_I18N})")

  add_custom_target(icu4c)

  add_library(icuuc INTERFACE)
  target_include_directories(icuuc INTERFACE ${LIBICU_INCLUDE})
  target_link_libraries(icuuc INTERFACE ${LIBICU_UC})

  add_library(icui18n INTERFACE)
  target_include_directories(icui18n INTERFACE ${LIBICU_INCLUDE})
  target_link_libraries(icui18n INTERFACE ${LIBICU_I18N})

  add_library(icuio INTERFACE)
  target_include_directories(icuio INTERFACE ${LIBICU_INCLUDE})
  target_link_libraries(icuio INTERFACE ${LIBICU_IO})

  add_library(icutu INTERFACE)
  target_include_directories(icutu INTERFACE ${LIBICU_INCLUDE})
  target_link_libraries(icutu INTERFACE ${LIBICU_TU})

  add_library(icudata INTERFACE)
  target_include_directories(icudata INTERFACE ${LIBICU_INCLUDE})
  target_link_libraries(icudata INTERFACE ${LIBICU_DATA})

  set(ICU4C_LIBRARIES
    ${LIBICU_UC}
    ${LIBICU_I18N}
    ${LIBICU_IO}
    ${LIBICU_TU}
    ${LIBICU_DATA}
    CACHE STRING "")

  return()
endif()

if(DISABLE_TP_ICU4C)
  message(FATAL_ERROR "icu4c not found and third-party disabled")
endif()

include(ExternalProject)

message("Using third-party icu4c")

set(ICU_FLAGS "-DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1 -DU_USING_ICU_NAMESPACE=0 -fPIC")
if (APPLE)
  set(ICU_FLAGS "${ICU_FLAGS} -DU_CHARSET_IS_UTF8=1")
endif()

set(ICU4C_PREFIX ${CMAKE_BINARY_DIR}/third-party/install)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config.sh
  export " " CPPFLAGS="${ICU_FLAGS}" " " CFLAGS="${ICU_FLAGS}" \n
  ${CMAKE_CURRENT_SOURCE_DIR}/src/icu4c/source/runConfigureICU " " Linux " " \\\n
  --disable-shared " " \\\n
  --enable-static " " \\\n
  --prefix=${ICU4C_PREFIX} " " \\\n
  --disable-tests " " \\\n
  --disable-samples \n
  )

if(CMAKE_GENERATOR STREQUAL "Ninja")
  set(MAKE make --max-load 8)
else()
  set(MAKE $(MAKE))
endif()

ExternalProject_Add(
  icu4c
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/icu4c/source
  BUILD_IN_SOURCE 0
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/build
  CONFIGURE_COMMAND bash ${CMAKE_CURRENT_BINARY_DIR}/config.sh CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
  BUILD_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} ${MAKE}
  INSTALL_DIR ${ICU4C_PREFIX}
  INSTALL_COMMAND ${MAKE} install
  BUILD_BYPRODUCTS
  ${ICU4C_PREFIX}/lib/libicuuc.a
  ${ICU4C_PREFIX}/lib/libicui18n.a
  ${ICU4C_PREFIX}/lib/libicuio.a
  ${ICU4C_PREFIX}/lib/libicutu.a
  ${ICU4C_PREFIX}/lib/libicudata.a
  )

add_library(icuuc INTERFACE)
add_dependencies(icuuc icu4c)
target_include_directories(icuuc INTERFACE ${ICU4C_PREFIX}/include)
target_link_libraries(icuuc INTERFACE ${ICU4C_PREFIX}/lib/libicuuc.a)

add_library(icui18n INTERFACE)
add_dependencies(icui18n icu4c)
target_include_directories(icui18n INTERFACE ${ICU4C_PREFIX}/include)
target_link_libraries(icui18n INTERFACE ${ICU4C_PREFIX}/lib/libicui18n.a)

add_library(icuio INTERFACE)
add_dependencies(icuio icu4c)
target_include_directories(icuio INTERFACE ${ICU4C_PREFIX}/include)
target_link_libraries(icuio INTERFACE ${ICU4C_PREFIX}/lib/libicuio.a)

add_library(icutu INTERFACE)
add_dependencies(icutu icu4c)
target_include_directories(icutu INTERFACE ${ICU4C_PREFIX}/include)
target_link_libraries(icutu INTERFACE ${ICU4C_PREFIX}/lib/libicutu.a)

add_library(icudata INTERFACE)
add_dependencies(icudata icu4c)
target_include_directories(icudata INTERFACE ${ICU4C_PREFIX}/include)
target_link_libraries(icudata INTERFACE ${ICU4C_PREFIX}/lib/libicudata.a)

set(ICU4C_LIBRARIES
  ${ICU4C_PREFIX}/lib/libicuuc.a
  ${ICU4C_PREFIX}/lib/libicui18n.a
  ${ICU4C_PREFIX}/lib/libicuio.a
  ${ICU4C_PREFIX}/lib/libicutu.a
  ${ICU4C_PREFIX}/lib/libicudata.a
  CACHE STRING "")
