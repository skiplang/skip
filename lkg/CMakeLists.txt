if(EXTERN_LKG)
  message("************************************************************************")
  message("************************************************************************")
  message("** Using external LKG: ${EXTERN_LKG} **")
  message("************************************************************************")
  message("************************************************************************")


  if(NOT EXISTS ${EXTERN_LKG}/bin/skip_to_llvm)
    message(FATAL_ERROR "${EXTERN_LKG}/bin/skip_to_llvm not found.  Did you already build skip_to_llvm in your external LKG?")
  endif()

  # When using an external LKG we don't want to actually depend on it - just use
  # it.  It's up to the user to rebuild it properly.

  add_executable(skip_to_llvm.lkg IMPORTED GLOBAL)
  set_target_properties(skip_to_llvm.lkg PROPERTIES
    IMPORTED_LOCATION ${EXTERN_LKG}/bin/skip_to_llvm
    OUTPUT_NAME skip_to_llvm
    RUNTIME_OUTPUT_DIRECTORY ${EXTERN_LKG}/bin
  )

  return()
endif()

set(PHASE lkg)
add_subdirectory(runtime/native)
set(PHASE)

recursiveGlobSources(EXTC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/include/skip/*-extc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/include/skip/plugin/skip_plugin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/include/skip/rvmi/rvmi.h
)

set(preamble_ll ${CMAKE_CURRENT_BINARY_DIR}/tmp/preamble.ll)

add_custom_command(
  OUTPUT ${preamble_ll}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/gen_preamble
  preamble
  --compiler ${CLANG_EXECUTABLE}
    -o ${preamble_ll}
  $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:--ndebug>
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/gen_preamble ${EXTC_FILES}
)


add_custom_target(lkg)

set(ll ${CMAKE_CURRENT_SOURCE_DIR}/skip_to_llvm.ll.bz2)
set(gen_ll ${CMAKE_CURRENT_BINARY_DIR}/tmp/skip_to_llvm.gen.ll)
set(gen_o ${CMAKE_CURRENT_BINARY_DIR}/tmp/skip_to_llvm.gen.o)

add_custom_command(
  OUTPUT ${gen_ll}
  COMMAND cat ${preamble_ll} > ${gen_ll}
  COMMAND bzcat ${ll} >> ${gen_ll}
  DEPENDS ${preamble_ll} ${ll}
  )

add_custom_command(
  OUTPUT ${gen_o}
  COMMAND echo Building LKG skip_to_llvm.o
  COMMAND ${CLANG_EXECUTABLE} -fPIC -c -o ${gen_o} ${gen_ll}
  DEPENDS ${gen_ll}
  )

add_executable(skip_to_llvm.lkg
  ${gen_o}
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/src/sk_standalone.cpp
  )
set_target_properties(skip_to_llvm.lkg
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
  OUTPUT_NAME skip_to_llvm
  )
target_link_libraries(skip_to_llvm.lkg skip_runtime.lkg)

#add_command_target(
#  skip_to_llvm.lkg
#  OUTPUT ${binary_name}
#  COMMAND echo Building LKG skip_to_llvm
#  COMMAND
#  ${CLANG_EXECUTABLE}
#  -o ${binary_name}
#  ${gen_ll}
#  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/src/sk_standalone.cpp
#  `pkg-config --cflags --libs ${CMAKE_CURRENT_BINARY_DIR}/runtime/native/native_cc.pc`
#  -pthread
#  DEPENDS
#  skip_runtime.lkg ${gen_ll} native_cc.lkg
#  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/native/src/sk_standalone.cpp
#  )

add_dependencies(lkg skip_to_llvm.lkg)
