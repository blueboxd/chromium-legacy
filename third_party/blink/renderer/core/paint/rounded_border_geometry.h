// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class FloatRoundedRect;
class LayoutRectOutsets;
struct PhysicalRect;

class CORE_EXPORT RoundedBorderGeometry {
  STATIC_ONLY(RoundedBorderGeometry);

 public:
  static FloatRoundedRect RoundedBorder(const ComputedStyle&,
                                        const PhysicalRect& border_rect);

  static FloatRoundedRect PixelSnappedRoundedBorder(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true);

  static FloatRoundedRect RoundedInnerBorder(const ComputedStyle&,
                                             const PhysicalRect& border_rect);

  static FloatRoundedRect PixelSnappedRoundedInnerBorder(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true);

  static FloatRoundedRect PixelSnappedRoundedInnerBorder(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      const LayoutRectOutsets& insets,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_
