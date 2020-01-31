if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(USE_JEMALLOC "Use jemalloc" ON)

# Quiet make output
set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

option(INCLUDE_FAILING "Include failing tests" OFF)

option(EXTERN_LKG "Location of an external LKG. If set should be the path to another skip repo's build tree to use as the lkg." OFF)

option(CONFIGURE_TESTS "Configure including tests" ON)

option(DISABLE_THIRD_PARTY "Disable all built-in third-party submodules" OFF)
option(DISABLE_THIRD_PARTY_UPDATE "Disable automatic update of third-party" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_GFLAGS "Disable built-in gflags" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_GTEST "Disable built-in gtest" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_ICU4C "Disable built-in icu4c" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_JEMALLOC "Disable built-in jemalloc" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_LIBUNWIND "Disable built-in libunwind" ${DISABLE_THIRD_PARTY})
option(DISABLE_TP_PCRE "Disable built-in pcre" ${DISABLE_THIRD_PARTY})
