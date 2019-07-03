// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_OCCLUSION_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_OCCLUSION_STATE_H_

#include <stdint.h>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Indicates whether a child frame is occluded or visually altered by content
// or styles in the parent frame.
enum class FrameOcclusionState {
  // No occlusion determination was made.
  kUnknown = 0,
  // The frame *may* be occluded or visually altered.
  kPossiblyOccluded = 1,
  // The frame is definitely not occluded or visually altered.
  kGuaranteedNotOccluded = 2,
  kMaxValue = kGuaranteedNotOccluded,
};

// These values are used to implement a browser intervention: if a cross-
// origin iframe has moved more than 30 screen pixels (manhattan distance)
// within its embedding page's viewport within the last 500 milliseconds, most
// input events targeting the iframe will be quietly discarded.
static const uint32_t kMaxChildFrameScreenRectMovement = 30;
static const uint32_t kMinScreenRectStableTimeMs = 500;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_OCCLUSION_STATE_H_
