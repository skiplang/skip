# These are required by the native compiler
# TODO: Move these to the runtime/native directory.

# Binary path where executables will be deposited
set(SKIP_BIN_PATH ${CMAKE_BINARY_DIR}/bin)

set(SKIP_TOOLS_PATH ${CMAKE_SOURCE_DIR}/tools)

set(SIMPLE_PASSING_TESTRES "\\{\\\"testHasExpect\\\": false, \\\"returncode\\\": 0, \\\"expectError\\\": false, \\\"testReportsOk\\\": false\}")
