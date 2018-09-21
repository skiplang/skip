set(COMMON_ARGS
  BACKENDS js
  RUNNER ${CMAKE_SOURCE_DIR}/prelude/testing/run_test
  RELATIVE ${CMAKE_SOURCE_DIR})

# TODO: This test has badly bitrotted, but the idea is useful. Can we save it?
# At the very least we need some convenient way to pull in pretty much the
# entire rest of the compiler, since vtable.sk now transitively depends on
# everything.
#
# add_skip_test(${CMAKE_CURRENT_SOURCE_DIR}/test_color.sk ${COMMON_ARGS} LIBS ${CMAKE_SOURCE_DIR}/src/native/vtable.sk)
