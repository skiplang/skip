# These are all the prelude files - some are shadows for others so this list is
# only valid for dependency testing.
recursiveGlobSources(PRELUDE_ALL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.sk)

# These are the prelude files specific to compiling with the native backend
fetch_depends(PRELUDE_SOURCES native ${CMAKE_CURRENT_SOURCE_DIR})
set(PRELUDE_SOURCES ${PRELUDE_SOURCES} PARENT_SCOPE)
