# ------------------------------------------------------------------------------
# Native Backend

list(APPEND COMPILER_SOURCES
  ../outer/outerIst.sk
  ../outer/skipLowerMatches.sk
  ../outer/skipLowerOuterIst.sk
  ../outer/skipMakeOuterIst.sk
  ../outer/skipOptimizeLocals.sk
  ../outer/skipOuterIstPp.sk
  ../outer/skipOuterIstUtils.sk
  ../outer/skipRenameLocals.sk
  ../outer/skipValidateOuterIst.sk
  ../project/ProjectConfig.sk
  ../project/SkipProject.sk
  ../project/SolutionLoader.sk
  ../project/ConfigSerialization.sk
  AsmOutput.sk
  Config.sk
  Emitter.sk
  FunctionTree.sk
  GIR.sk
  IR.sk
  Layout.sk
  NameMangler.sk
  OuterIstToIR.sk
  PrettyIR.sk
  Rewrite.sk
  StmtPos.sk
  TextOutputStream.sk
  alloc.sk
  callables.sk
  checksum.sk
  compile.sk
  const.sk
  control.sk
  coroutine.sk
  deadcode.sk
  dominator.sk
  escape.sk
  fundeps.sk
  gc.sk
  inline.sk
  intrinsics.sk
  ipa.sk
  lower.sk
  main.sk
  optimize.sk
  optinfo.sk
  peephole.sk
  regpromote.sk
  safepoint.sk
  specialize.sk
  types.sk
  util.sk
  verify.sk
  vtable.sk
  hhvm/gather.sk
  hhvm/hhvm.sk
  hhvm/propertyGetHelper.sk
  hhvm/propertySetHelper.sk
  hhvm/typeTable.sk
  )

# Make paths absolute.
prependEach(COMPILER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/")

set(COMPILER_SOURCES ${COMPILER_SOURCES} PARENT_SCOPE)

add_skip_compiler_executable(skip_to_llvm
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_llvm
  DEPENDS
  ${COMPILER_SOURCES}
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES}
  )
install(TARGETS skip_to_llvm RUNTIME DESTINATION bin)

# Single .ll skip_to_llvm used for update_lkg_compiler
add_skip_compiler_executable(
  lkg_prep_skip_to_llvm
  ${CMAKE_CURRENT_SOURCE_DIR}:skip_to_llvm
  EXCLUDE_FROM_ALL
  LLVM_TEMP_PATH ${CMAKE_CURRENT_BINARY_DIR}/lkg_prep_skip_to_llvm.ll
  DEPENDS
  ${COMPILER_SOURCES}
  ${FRONTEND_SOURCES}
  ${UTILS_SOURCES}
  ${PRELUDE_SOURCES}
  )

add_subdirectory(tests)
