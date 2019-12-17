// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/ua_style.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"

namespace blink {

namespace {

LayoutUnit BorderWidth(LayoutUnit width, EBorderStyle style) {
  if (style == EBorderStyle::kNone || style == EBorderStyle::kHidden) {
    return LayoutUnit(0);
  }
  return width;
}

}  // namespace

UAStyle::UAStyle()
    : top_left_(ComputedStyleInitialValues::InitialBorderTopLeftRadius()),
      top_right_(ComputedStyleInitialValues::InitialBorderTopRightRadius()),
      bottom_left_(ComputedStyleInitialValues::InitialBorderBottomLeftRadius()),
      bottom_right_(
          ComputedStyleInitialValues::InitialBorderBottomRightRadius()),
      border_left_color_(ComputedStyleInitialValues::InitialBorderColor()),
      border_right_color_(ComputedStyleInitialValues::InitialBorderColor()),
      border_top_color_(ComputedStyleInitialValues::InitialBorderColor()),
      border_bottom_color_(ComputedStyleInitialValues::InitialBorderColor()),
      border_left_style_(ComputedStyleInitialValues::InitialBorderLeftStyle()),
      border_right_style_(
          ComputedStyleInitialValues::InitialBorderRightStyle()),
      border_top_style_(ComputedStyleInitialValues::InitialBorderTopStyle()),
      border_bottom_style_(
          ComputedStyleInitialValues::InitialBorderBottomStyle()),
      border_left_width_(ComputedStyleInitialValues::InitialBorderLeftWidth()),
      border_right_width_(
          ComputedStyleInitialValues::InitialBorderRightWidth()),
      border_top_width_(ComputedStyleInitialValues::InitialBorderTopWidth()),
      border_bottom_width_(
          ComputedStyleInitialValues::InitialBorderBottomWidth()),
      border_image_(ComputedStyleInitialValues::InitialBorderImage()),
      background_layers_(ComputedStyleInitialValues::InitialBackground()),
      background_color_(ComputedStyleInitialValues::InitialBackgroundColor()) {}

bool UAStyle::HasDifferentBackground(const ComputedStyle& other) const {
  FillLayer other_background_layers = other.BackgroundLayers();
  // Exclude background-repeat from comparison by resetting it.
  other_background_layers.SetRepeatX(
      FillLayer::InitialFillRepeatX(EFillLayerType::kBackground));
  other_background_layers.SetRepeatY(
      FillLayer::InitialFillRepeatY(EFillLayerType::kBackground));

  return (!background_layers_.VisuallyEqual(other_background_layers) ||
          background_color_ != other.BackgroundColor());
}

bool UAStyle::HasDifferentBorder(const ComputedStyle& other) const {
  return !border_image_.DataEquals(other.BorderImage()) ||
         !BorderColorEquals(other) || !BorderWidthEquals(other) ||
         !BorderRadiiEquals(other) || !BorderStyleEquals(other);
}

bool UAStyle::BorderColorEquals(const ComputedStyle& other) const {
  return border_left_color_ == other.BorderLeftColor() &&
         border_right_color_ == other.BorderRightColor() &&
         border_top_color_ == other.BorderTopColor() &&
         border_bottom_color_ == other.BorderBottomColor();
}

bool UAStyle::BorderWidthEquals(const ComputedStyle& other) const {
  return (BorderWidth(border_left_width_, border_left_style_) ==
              other.BorderLeftWidth() &&
          BorderWidth(border_right_width_, border_right_style_) ==
              other.BorderRightWidth() &&
          BorderWidth(border_top_width_, border_top_style_) ==
              other.BorderTopWidth() &&
          BorderWidth(border_bottom_width_, border_bottom_style_) ==
              other.BorderBottomWidth());
}

bool UAStyle::BorderRadiiEquals(const ComputedStyle& other) const {
  return top_left_ == other.BorderTopLeftRadius() &&
         top_right_ == other.BorderTopRightRadius() &&
         bottom_left_ == other.BorderBottomLeftRadius() &&
         bottom_right_ == other.BorderBottomRightRadius();
}

bool UAStyle::BorderStyleEquals(const ComputedStyle& other) const {
  return (border_left_style_ == other.BorderLeftStyle() &&
          border_right_style_ == other.BorderRightStyle() &&
          border_top_style_ == other.BorderTopStyle() &&
          border_bottom_style_ == other.BorderBottomStyle());
}

}  // namespace blink
