// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"

namespace blink {

LayoutUnit NGPhysicalStaticPosition::LeftInset(LayoutUnit container_size,
                                               LayoutUnit width,
                                               LayoutUnit margin_left,
                                               LayoutUnit margin_right) const {
  if (HasLeft())
    return offset.left;
  else
    return offset.left - width - margin_left - margin_right;
}

LayoutUnit NGPhysicalStaticPosition::RightInset(LayoutUnit container_size,
                                                LayoutUnit width,
                                                LayoutUnit margin_left,
                                                LayoutUnit margin_right) const {
  if (HasLeft())
    return container_size - offset.left - width - margin_left - margin_right;
  else
    return container_size - offset.left;
}

LayoutUnit NGPhysicalStaticPosition::TopInset(LayoutUnit container_size,
                                              LayoutUnit height,
                                              LayoutUnit margin_top,
                                              LayoutUnit margin_bottom) const {
  if (HasTop())
    return offset.top;
  else
    return offset.top - height - margin_bottom - margin_top;
}

LayoutUnit NGPhysicalStaticPosition::BottomInset(
    LayoutUnit container_size,
    LayoutUnit height,
    LayoutUnit margin_top,
    LayoutUnit margin_bottom) const {
  if (HasTop())
    return container_size - offset.top - height - margin_top - margin_bottom;
  else
    return container_size - offset.top;
}

}  // namespace blink
