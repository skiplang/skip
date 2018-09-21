/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

//===- lli.cpp - LLVM Interpreter / Dynamic compiler ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility provides a simple wrapper around the LLVM Execution Engines,
// which allow the direct execution of LLVM programs through a Just-In-Time
// compiler, or through an interpreter if no JIT is available for this platform.
//
//===----------------------------------------------------------------------===//

#include "OrcLazyJIT.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/ObjectCache.h"
#include "llvm/ExecutionEngine/OrcMCJITReplacement.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/OrcRemoteTargetClient.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include <cerrno>

#include "skip/external.h"
#include "skip/String.h"
#include "skip/System.h"
#include "skip/Obstack.h"

#include "skip/File-extc.h"
#include "skip/String-extc.h"
#include "skip/memoize-extc.h"
#include "skip/Obstack-extc.h"
#include "skip/System-extc.h"
#include "skip/objects-extc.h"
#include "skip/SkipMath-extc.h"
#include "skip/Type-extc.h"
#include "skip/parallel-extc.h"
#include "skip/SkipRegex-extc.h"
#include "skip/intern-extc.h"
#include "skip/stubs-extc.h"
#include "skip/plugin-extc.h"

#include <folly/init/Init.h>

using namespace llvm;

//===----------------------------------------------------------------------===//

CodeGenOpt::Level getOptLevel() {
  return CodeGenOpt::Default; // None, Less, Default, Aggressive
}

LLVM_ATTRIBUTE_NORETURN
static void reportError(SMDiagnostic Err, const char *ProgName) {
  Err.print(ProgName, errs());
  exit(1);
}

//===----------------------------------------------------------------------===//
// main Driver function
//

std::unique_ptr<OrcLazyJIT> J;

template <typename PtrTy>
static PtrTy fromTargetAddress(JITTargetAddress Addr) {
  return reinterpret_cast<PtrTy>(static_cast<uintptr_t>(Addr));
}

#define FORWARD(NAME, RT, PARAMS, CALL)     \
RT NAME PARAMS { \
  if (auto sym = J->findSymbol(#NAME)) { \
    using FnPtr = decltype(NAME); \
    auto fn = fromTargetAddress<FnPtr*>(cantFail(sym.getAddress())); \
    return fn CALL; \
  } else if (auto Err = sym.takeError()) { \
    logAllUnhandledErrors(std::move(Err), llvm::errs(), ""); \
  } else {                                                       \
    fprintf(stderr, "*** Lookup(1) failed:Could not find %s function.\n", #NAME); \
  }                                                              \
  abort();                                      \
}

#define FORWARD_NR(NAME, RT, PARAMS, CALL)     \
RT NAME PARAMS { \
  if (auto sym = J->findSymbol(#NAME)) { \
    using FnPtr = decltype(NAME); \
    auto fn = fromTargetAddress<FnPtr*>(cantFail(sym.getAddress()));    \
    fn CALL;                                                            \
  } else if (auto Err = sym.takeError()) { \
    logAllUnhandledErrors(std::move(Err), llvm::errs(), ""); \
  } else {                                                       \
    fprintf(stderr, "*** Lookup(2) failed:Could not find %s function.\n", #NAME); \
  }                                                              \
  abort();                                      \
}

FORWARD(SKIP_getExceptionMessage, SkipString, (SkipRObj* skipException), (skipException));
FORWARD_NR(SKIP_throwRuntimeError, void, (SkipString message), (message))
FORWARD(SKIP_createStringVector, SkipRObj*, (int64_t size), (size))
FORWARD(SKIP_createIntVector, SkipRObj*, (int64_t size), (size))
FORWARD_NR(SKIP_throwInvariantViolation, void, (SkipString msg), (msg))
FORWARD(SKIP_createMixedBool, SkipRObj*, (bool value), (value))
FORWARD(SKIP_createMixedFloat, SkipRObj*, (double value), (value))
FORWARD(SKIP_createMixedInt, SkipRObj*, (int64_t value), (value))
FORWARD(SKIP_createMixedNull, SkipRObj*, (void), ())
FORWARD(SKIP_createMixedString, SkipRObj*, (SkipString value), (value))
FORWARD(SKIP_createMixedDict, SkipRObj*, (int64_t capacity), (capacity))
FORWARD(SKIP_MixedDict_set, void, (SkipRObj* obj, SkipString key, SkipRObj* value), (obj, key, value))
FORWARD(SKIP_MixedDict_freeze, SkipRObj*, (SkipRObj* obj), (obj))
FORWARD(SKIP_createMixedVec, SkipRObj*, (int64_t capacity), (capacity))
FORWARD(SKIP_MixedVec_push, void, (SkipRObj* obj, SkipRObj* value), (obj, value))
FORWARD(SKIP_MixedVec_freeze, SkipRObj*, (SkipRObj* obj), (obj))
FORWARD(SKIP_initializeSkip, const svmi::FunctionSignature*, (void), ());
FORWARD(skip_main, SkipString, (void), ());

#undef FORWARD
#undef FORWARD_NR

void SKIP_HHVM_incref(SkipHhvmHandle* wrapper) { abort(); }
void SKIP_HHVM_decref(SkipHhvmHandle* wrapper) { abort(); }

void* SKIP_lookup(const std::string& name) {
  //@_ZTIN4skip13SkipExceptionE = external constant { i8*, i8*, i8* }, align 8
#define FORWARD(x) do { if (name == "_" #x) { return (void*)x; } } while (0)
  FORWARD(SKIP_AssocPresenceBitsUtils___BaseMetaImpl__storeAPBMapping);
  FORWARD(SKIP_Debug_getMemoryFrameUsage);
  FORWARD(SKIP_Debug_printBoxedObjectSize);
  FORWARD(SKIP_Debug_printMemoryStatistics);
  FORWARD(SKIP_FBTraceGlueTaoExtension__fireRprecv);
  FORWARD(SKIP_FBTraceGlueTaoExtension__fireRqsend);
  FORWARD(SKIP_FileSystem_ensure_directory);
  FORWARD(SKIP_FileSystem_exists);
  FORWARD(SKIP_FileSystem_is_directory);
  FORWARD(SKIP_FileSystem_readdir);
  FORWARD(SKIP_Array_concatStringArray);
  FORWARD(SKIP_FlightTrackerTicket__toJsonString);
  FORWARD(SKIP_Float_toString);
  //  FORWARD(SKIP_HHVM_Object_decref);
  //  FORWARD(SKIP_HHVM_Object_getField_Bool);
  //  FORWARD(SKIP_HHVM_Object_getField_Float);
  //  FORWARD(SKIP_HHVM_Object_getField_Int);
  //  FORWARD(SKIP_HHVM_Object_getField_Object);
  //  FORWARD(SKIP_HHVM_Object_getField_String);
  //  FORWARD(SKIP_HHVM_Object_incref);
  //  FORWARD(SKIP_HHVM_Object_setField_Bool);
  //  FORWARD(SKIP_HHVM_Object_setField_Float);
  //  FORWARD(SKIP_HHVM_Object_setField_Int);
  //  FORWARD(SKIP_HHVM_Object_setField_Object);
  //  FORWARD(SKIP_HHVM_Object_setField_String);
  //  FORWARD(SKIP_HHVM_callFunction);
  //  FORWARD(SKIP_HHVM_throwException);
  //  FORWARD(SKIP_HhvmStringRet_create);
  FORWARD(SKIP_Int_toString);
  FORWARD(SKIP_Math_acos);
  FORWARD(SKIP_Math_asin);
  FORWARD(SKIP_Obstack_alloc);
  FORWARD(SKIP_Obstack_calloc);
  FORWARD(SKIP_Obstack_collect);
  FORWARD(SKIP_Obstack_collect0);
  FORWARD(SKIP_Obstack_collect1);
  FORWARD(SKIP_Obstack_freeze);
  FORWARD(SKIP_Obstack_note_inl);
  FORWARD(SKIP_Obstack_shallowClone);
  FORWARD(SKIP_Obstack_usage);
  FORWARD(SKIP_Obstack_verifyStore);
  FORWARD(SKIP_Obstack_wrapHhvmObject);
  FORWARD(SKIP_Obstack_wrapHhvmObjectData);
  FORWARD(SKIP_Regex_initialize);
  FORWARD(SKIP_String_StringIterator__rawCurrent);
  FORWARD(SKIP_String_StringIterator__rawDrop);
  FORWARD(SKIP_String_StringIterator__rawNext);
  FORWARD(SKIP_String_StringIterator__substring);
  FORWARD(SKIP_String__byteLength);
  FORWARD(SKIP_String__fromChars);
  FORWARD(SKIP_String__length);
  FORWARD(SKIP_String__matchInternal);
  FORWARD(SKIP_String__sliceByteOffsets);
  FORWARD(SKIP_String__toFloat_raw);
  FORWARD(SKIP_String__unsafe_get);
  FORWARD(SKIP_String_cmp);
  FORWARD(SKIP_String_concat);
  FORWARD(SKIP_String_concat2);
  FORWARD(SKIP_String_concat3);
  FORWARD(SKIP_String_concat4);
  FORWARD(SKIP_String_eq);
  FORWARD(SKIP_String_hash);
  FORWARD(SKIP_String_longStringEq);
  FORWARD(SKIP_String_toIntOptionHelper);
  FORWARD(SKIP_TaoClientErrors___BaseMetaImpl__getLogger);
  FORWARD(SKIP_TaoClient__changeState);
  FORWARD(SKIP_TaoRequestUtils___BaseMetaImpl__isArrayResult);
  FORWARD(SKIP_TaoServerError__create);
  FORWARD(SKIP_TaoServerError__getPathFromMessage);
  FORWARD(SKIP_TaoServerError__shouldFetchIP);
  FORWARD(SKIP_TicketUtils__ticketToGtid);
  FORWARD(SKIP_TicketUtils__ticketToGtidFallback);
  FORWARD(SKIP_arguments);
  FORWARD(SKIP_debug_break);
  FORWARD(SKIP_getcwd);
  FORWARD(SKIP_intern);
  FORWARD(SKIP_internalExit);
  FORWARD(SKIP_is_push_phase2_experimental_machine);
  FORWARD(SKIP_memoizeCallReturningFloat);
  FORWARD(SKIP_memoizeCallReturningInt);
  FORWARD(SKIP_memoizeCallReturningObject);
  FORWARD(SKIP_memoizeCallReturningString);
  FORWARD(SKIP_memoizeReturnFloat);
  FORWARD(SKIP_memoizeReturnInt);
  FORWARD(SKIP_memoizeReturnObject);
  FORWARD(SKIP_memoizeReturnString);
  FORWARD(SKIP_memoizeThrow);
  FORWARD(SKIP_now);
  FORWARD(SKIP_open_file);
  FORWARD(SKIP_parallelTabulate);
  FORWARD(SKIP_print_error);
  FORWARD(SKIP_print_last_exception_stack_trace_and_exit);
  FORWARD(SKIP_print_raw);
  FORWARD(SKIP_flush_stdout);
  FORWARD(SKIP_profile_report);
  FORWARD(SKIP_profile_start);
  FORWARD(SKIP_profile_stop);
  //  FORWARD(SKIP_string_create);
//  FORWARD(SKIP_string_create_cstr);
//  FORWARD(SKIP_string_extractData);
  FORWARD(SKIP_string_to_file);
  FORWARD(SKIP_throw);
  FORWARD(SKIP_unreachable);
  FORWARD(SKIP_unreachableMethodCall);
  FORWARD(SKIP_unreachableWithExplanation);
  FORWARD(abort);
#undef FORWARD

  fprintf(stderr, "SKIP_lookup: %s\n", name.c_str());
  abort();
}



int main(int argc, char** argv) {
  int follyArgc = 1;
  std::array<char*, 1> follyArgv{{argv[0]}};
  char** pfollyArgv = follyArgv.data();
  folly::init(&follyArgc, &pfollyArgv, false);

  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);

  atexit(llvm_shutdown); // Call llvm_shutdown() on exit.

  // If we have a native target, initialize it to ensure it is linked in and
  // usable by the JIT.
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  sys::Process::PreventCoreFiles();

  if (argc < 2) {
    fprintf(stderr, "Too few parameters\n");
    return 1;
  }

  auto module = argv[1];
  argc--;
  for (int i = 1; i <= argc; ++i) {
    argv[i] = argv[i + 1];
  }

  skip::initializeSkip(argc, argv, false/*watch*/);

  LLVMContext Context;
  SMDiagnostic Err;
  std::unique_ptr<Module> Owner = parseIRFile(module, Err, Context);
  Module *Mod = Owner.get();
  if (!Mod)
    reportError(Err, argv[0]);

  // Add the program's symbols into the JIT's search space.
  if (sys::DynamicLibrary::LoadLibraryPermanently(nullptr)) {
    errs() << "Error loading program symbols.\n";
    return 1;
  }

  // Grab a target machine and try to build a factory function for the
  // target-specific Orc callback manager.
  EngineBuilder EB;
  EB.setOptLevel(getOptLevel()); // None, Less, Default, Aggressive
  auto TM = std::unique_ptr<TargetMachine>(EB.selectTarget());
  Triple T(TM->getTargetTriple());
  auto CompileCallbackMgr = orc::createLocalCompileCallbackManager(T, 0);

  // If we couldn't build the factory function then there must not be a callback
  // manager for this target. Bail out.
  if (!CompileCallbackMgr) {
    errs() << "No callback manager available for target '"
           << TM->getTargetTriple().str() << "'.\n";
    return 1;
  }

  auto IndirectStubsMgrBuilder = orc::createLocalIndirectStubsManagerBuilder(T);

  // If we couldn't build a stubs-manager-builder for this target then bail out.
  if (!IndirectStubsMgrBuilder) {
    errs() << "No indirect stubs manager available for target '"
           << TM->getTargetTriple().str() << "'.\n";
    return 1;
  }

  // Everything looks good. Build the JIT.
 J = std::make_unique<OrcLazyJIT>(std::move(TM), std::move(CompileCallbackMgr),
                                  std::move(IndirectStubsMgrBuilder),
                                  true/*OrcInlineStubs*/);

  // Add the module, look up main and run it.
  //  for (auto &M : Ms)
  //    cantFail(J.addModule(std::shared_ptr<Module>(std::move(M))));
  cantFail(J->addModule(std::move(Owner)));

  try {
    {
      skip::Obstack::PosScope P;
      fprintf(stderr, "<SKIPI> running SKIP_initializeSkip\n");
      SKIP_initializeSkip();
    }

    {
      skip::Obstack::PosScope P;
      fprintf(stderr, "<SKIPI> running skip_main\n");
      SkipString s = skip_main();
      fprintf(stderr, "<SKIPI> skip_main finished\n");
      skip::String::CStrBuffer buf;
      printf("<SKIPI> skip_main returned: %s\n", skip::String{s}.c_str(buf));
    }
  } catch(skip::SkipExitException& e) {
    // Because we were handling an exception the PosScopes above didn't clear
    // the Obstack - so we need to force clear to ensure the Obstack is cleared
    // before we exit.
    skip::Obstack::cur().clearAll();
    fprintf(stderr, "Caught SkipExitException!\n");
    return e.m_status;
  }

  J.reset();
  return 0;
}
