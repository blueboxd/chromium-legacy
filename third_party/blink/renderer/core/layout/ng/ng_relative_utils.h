// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGRelativeUtils_h
#define NGRelativeUtils_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class ComputedStyle;
struct PhysicalOffset;

// Implements the relative positioning spec:
// https://www.w3.org/TR/css-position-3/#rel-pos
// Returns the relative position offset as defined by |child_style|.
CORE_EXPORT PhysicalOffset
ComputeRelativeOffset(const ComputedStyle& child_style,
                      WritingMode container_writing_mode,
                      TextDirection container_direction,
                      PhysicalSize container_size);

}  // namespace blink

#endif  // NGRelativeUtils_h
