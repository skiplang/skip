/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/stubs-extc.h"

#include <cstdio>
#include <cstdlib>

#define MAKE_STUB(NAME, RET, ARGS)                       \
  RET NAME ARGS {                                        \
    fprintf(stderr, "Invalid call to stub %s\n", #NAME); \
    abort();                                             \
  }

MAKE_STUB(
    SKIP_AssocPresenceBitsUtils___BaseMetaImpl__storeAPBMapping,
    void,
    (void*, int64_t, void*));
MAKE_STUB(
    SKIP_FBTraceGlueTaoExtension__fireRprecv,
    void,
    (void*, void*, void*));
MAKE_STUB(SKIP_FBTraceGlueTaoExtension__fireRqsend, void, (void*, void*));
MAKE_STUB(
    SKIP_TaoClientErrors___BaseMetaImpl__getLogger,
    void*,
    (void*, int64_t, void*));
MAKE_STUB(SKIP_TaoClient__changeState, void, (void*));
MAKE_STUB(
    SKIP_TaoRequestUtils___BaseMetaImpl__isArrayResult,
    bool,
    (void*, void*));
MAKE_STUB(
    SKIP_TaoServerError__create,
    void*,
    (void*, void*, void*, void*, void*, void*, void*, bool));
MAKE_STUB(SKIP_TaoServerError__getPathFromMessage, void*, (void*, void*));
MAKE_STUB(SKIP_TaoServerError__shouldFetchIP, bool, (void*, void*));
MAKE_STUB(
    SKIP_TicketUtils__ticketToGtid,
    struct ticketToGtidRet,
    (void*, void*, int64_t));
MAKE_STUB(SKIP_is_push_phase2_experimental_machine, bool, (void));
MAKE_STUB(SKIP_FlightTrackerTicket__toJsonString, void*, (void*));
MAKE_STUB(
    SKIP_TicketUtils__ticketToGtidFallback,
    struct ticketToGtidRet,
    (void*, void*));
