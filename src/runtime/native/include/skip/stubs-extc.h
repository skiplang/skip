/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

// This should all be moved to custom .cpp files in www-shadow
extern void SKIP_AssocPresenceBitsUtils___BaseMetaImpl__storeAPBMapping(
    void*,
    int64_t,
    void*);
extern void SKIP_FBTraceGlueTaoExtension__fireRprecv(void*, void*, void*);
extern void SKIP_FBTraceGlueTaoExtension__fireRqsend(void*, void*);
extern void*
SKIP_TaoClientErrors___BaseMetaImpl__getLogger(void*, int64_t, void*);
extern void SKIP_TaoClient__changeState(void*);
extern bool SKIP_TaoRequestUtils___BaseMetaImpl__isArrayResult(void*, void*);
extern void* SKIP_TaoServerError__create(
    void*,
    void*,
    void*,
    void*,
    void*,
    void*,
    void*,
    bool);
extern void* SKIP_TaoServerError__getPathFromMessage(void*, void*);
extern bool SKIP_TaoServerError__shouldFetchIP(void*, void*);
struct ticketToGtidRet {
  void* a;
  void* b;
};
extern struct ticketToGtidRet
SKIP_TicketUtils__ticketToGtid(void*, void*, int64_t);
extern bool SKIP_is_push_phase2_experimental_machine(void);
extern void* SKIP_FlightTrackerTicket__toJsonString(void*);
extern struct ticketToGtidRet SKIP_TicketUtils__ticketToGtidFallback(
    void*,
    void*);
} // extern "C"
