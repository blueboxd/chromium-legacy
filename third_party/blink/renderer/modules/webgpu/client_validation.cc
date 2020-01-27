// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/client_validation.h"

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_copy_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

bool ValidateCopySize(UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
                      ExceptionState& exception_state) {
  if (copy_size.IsUnsignedLongSequence() &&
      copy_size.GetAsUnsignedLongSequence().size() != 3) {
    exception_state.ThrowRangeError("copySize length must be 3");
    return false;
  }
  return true;
}

bool ValidateTextureCopyView(GPUTextureCopyView* texture_copy_view,
                             ExceptionState& exception_state) {
  DCHECK(texture_copy_view);

  const UnsignedLongSequenceOrGPUOrigin3DDict origin =
      texture_copy_view->origin();
  if (origin.IsUnsignedLongSequence() &&
      origin.GetAsUnsignedLongSequence().size() != 3) {
    exception_state.ThrowRangeError(
        "texture copy view origin length must be 3");
    return false;
  }
  return true;
}

}  // namespace blink
