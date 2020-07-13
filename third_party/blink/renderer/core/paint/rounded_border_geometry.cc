// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

static FloatRoundedRect::Radii CalcRadiiFor(const ComputedStyle& style,
                                            FloatSize size) {
  return FloatRoundedRect::Radii(
      FloatSizeForLengthSize(style.BorderTopLeftRadius(), size),
      FloatSizeForLengthSize(style.BorderTopRightRadius(), size),
      FloatSizeForLengthSize(style.BorderBottomLeftRadius(), size),
      FloatSizeForLengthSize(style.BorderBottomRightRadius(), size));
}

FloatRoundedRect RoundedBorderGeometry::RoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_rect((FloatRect(border_rect)));
  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        CalcRadiiFor(style, FloatSize(border_rect.size));
    rounded_rect.IncludeLogicalEdges(radii, style.IsHorizontalWritingMode(),
                                     /*include_logical_left_edge*/ true,
                                     /*include_logical_right_edge*/ true);
    rounded_rect.ConstrainRadii();
  }
  return rounded_rect;
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  FloatRoundedRect rounded_rect(PixelSnappedIntRect(border_rect));
  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        CalcRadiiFor(style, FloatSize(border_rect.size));
    rounded_rect.IncludeLogicalEdges(radii, style.IsHorizontalWritingMode(),
                                     include_logical_left_edge,
                                     include_logical_right_edge);
    rounded_rect.ConstrainRadii();
  }
  return rounded_rect;
}

FloatRoundedRect RoundedBorderGeometry::RoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  int left_width = style.BorderLeftWidth();
  int right_width = style.BorderRightWidth();
  int top_width = style.BorderTopWidth();
  int bottom_width = style.BorderBottomWidth();

  LayoutRectOutsets insets(-top_width, -right_width, -bottom_width,
                           -left_width);

  PhysicalRect inner_rect(border_rect);
  inner_rect.Expand(insets);
  inner_rect.size.ClampNegativeToZero();

  FloatRoundedRect float_inner_rect((FloatRect(inner_rect)));

  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        RoundedBorder(style, border_rect).GetRadii();
    // Insets use negative values.
    radii.Shrink(-insets.Top().ToFloat(), -insets.Bottom().ToFloat(),
                 -insets.Left().ToFloat(), -insets.Right().ToFloat());
    float_inner_rect.IncludeLogicalEdges(radii, style.IsHorizontalWritingMode(),
                                         /*include_logical_left_edge*/ true,
                                         /*include_logical_right_edge*/ true);
  }
  return float_inner_rect;
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  bool horizontal = style.IsHorizontalWritingMode();

  int left_width = (!horizontal || include_logical_left_edge)
                       ? floorf(style.BorderLeftWidth())
                       : 0;
  int right_width = (!horizontal || include_logical_right_edge)
                        ? floorf(style.BorderRightWidth())
                        : 0;
  int top_width = (horizontal || include_logical_left_edge)
                      ? floorf(style.BorderTopWidth())
                      : 0;
  int bottom_width = (horizontal || include_logical_right_edge)
                         ? floorf(style.BorderBottomWidth())
                         : 0;

  return PixelSnappedRoundedInnerBorder(
      style, border_rect,
      LayoutRectOutsets(-top_width, -right_width, -bottom_width, -left_width),
      include_logical_left_edge, include_logical_right_edge);
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const LayoutRectOutsets& insets,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  PhysicalRect inner_rect = border_rect;
  inner_rect.Expand(insets);
  inner_rect.size.ClampNegativeToZero();

  // The standard LayoutRect::PixelSnappedIntRect() method will not
  // let small sizes snap to zero, but that has the side effect here of
  // preventing an inner border for a very thin element from snapping to
  // zero size as occurs when a unit width border is applied to a sub-pixel
  // sized element. So round without forcing non-near-zero sizes to one.
  FloatRoundedRect rounded_rect(IntRect(
      RoundedIntPoint(inner_rect.offset),
      IntSize(
          SnapSizeToPixelAllowingZero(inner_rect.Width(), inner_rect.X()),
          SnapSizeToPixelAllowingZero(inner_rect.Height(), inner_rect.Y()))));

  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        PixelSnappedRoundedBorder(style, border_rect).GetRadii();
    // Insets use negative values.
    radii.Shrink(-insets.Top().ToFloat(), -insets.Bottom().ToFloat(),
                 -insets.Left().ToFloat(), -insets.Right().ToFloat());
    rounded_rect.IncludeLogicalEdges(radii, style.IsHorizontalWritingMode(),
                                     include_logical_left_edge,
                                     include_logical_right_edge);
  }
  return rounded_rect;
}

}  // namespace blink
