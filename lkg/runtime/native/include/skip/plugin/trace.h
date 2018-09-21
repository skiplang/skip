/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace ext_skip {
namespace trace {

// These indexes must match between the tracing code in HHVM and the trace
// harness replaying the trace log.  The numbers themselves don't mean anything
// in particular.
enum class Command {
  cmd_return = 0,
  cmd_callFunction = 1,
  cmd_callImport = 2,

  cmd_gatherCollect = 9,

  cmd_objectCreate = 10,
  cmd_objectGetField = 11,
  cmd_objectSetField = 12,
  cmd_objectGetType = 13,

  cmd_objectCons_create = 14,
  cmd_objectCons_finish = 15,
  cmd_objectCons_setFieldMixed = 16,

  cmd_shapeCreate = 20,
  cmd_shapeGetField = 21,
  cmd_shapeSetField = 22,

  cmd_shapeCons_create = 25,
  cmd_shapeCons_finish = 26,
  cmd_shapeCons_setFieldMixed = 27,

  cmd_varray_internal_create = 50,
  cmd_varray_size = 51,
  cmd_varray_get = 52,
  cmd_varray_append = 53,
  cmd_varray_createFromFixedVector = 54,

  cmd_darray_internal_create = 60,
  cmd_darray_internal_set = 61,
  cmd_darray_iter_advance = 62,
  cmd_darray_iter_begin = 63,
  cmd_darray_iter_end = 64,
  cmd_darray_iter_get_key = 65,
  cmd_darray_iter_get_value = 66,

  cmd_keyset_internal_create = 70,
  cmd_keyset_internal_set = 71,

  cmd_maybe_convert_to_array = 200,
};
} // namespace trace
} // namespace ext_skip
