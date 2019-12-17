// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_UA_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_UA_STYLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/fill_layer.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;

// This class represents the computed values we _would_ have had for background
// and border properties had no user or author declarations existed. It is used
// by LayoutTheme::AdjustStyle to figure out if the author has styled a given
// form element.
class CORE_EXPORT UAStyle {
  USING_FAST_MALLOC(UAStyle);

 public:
  UAStyle();

  bool HasDifferentBackground(const ComputedStyle& other) const;
  bool HasDifferentBorder(const ComputedStyle& other) const;

  bool BorderColorEquals(const ComputedStyle& other) const;
  bool BorderWidthEquals(const ComputedStyle& other) const;
  bool BorderRadiiEquals(const ComputedStyle& other) const;
  bool BorderStyleEquals(const ComputedStyle& other) const;

  FillLayer& AccessBackgroundLayers() { return background_layers_; }
  void SetBackgroundColor(StyleColor c) { background_color_ = c; }
  void SetBorderBottomColor(StyleColor v) { border_bottom_color_ = v; }
  void SetBorderLeftColor(StyleColor v) { border_left_color_ = v; }
  void SetBorderRightColor(StyleColor v) { border_right_color_ = v; }
  void SetBorderTopColor(StyleColor v) { border_top_color_ = v; }
  void SetBorderBottomStyle(EBorderStyle s) { border_bottom_style_ = s; }
  void SetBorderLeftStyle(EBorderStyle s) { border_left_style_ = s; }
  void SetBorderRightStyle(EBorderStyle s) { border_right_style_ = s; }
  void SetBorderTopStyle(EBorderStyle s) { border_top_style_ = s; }
  void SetBorderBottomWidth(float w) { border_bottom_width_ = LayoutUnit(w); }
  void SetBorderLeftWidth(float w) { border_left_width_ = LayoutUnit(w); }
  void SetBorderRightWidth(float w) { border_right_width_ = LayoutUnit(w); }
  void SetBorderTopWidth(float w) { border_top_width_ = LayoutUnit(w); }
  void SetBorderTopLeftRadius(const LengthSize& v) { top_left_ = v; }
  void SetBorderTopRightRadius(const LengthSize& v) { top_right_ = v; }
  void SetBorderBottomLeftRadius(const LengthSize& v) { bottom_left_ = v; }
  void SetBorderBottomRightRadius(const LengthSize& v) { bottom_right_ = v; }
  const NinePieceImage& BorderImage() const { return border_image_; }
  void SetBorderImage(const NinePieceImage& v) { border_image_ = v; }

 private:
  LengthSize top_left_;
  LengthSize top_right_;
  LengthSize bottom_left_;
  LengthSize bottom_right_;
  StyleColor border_left_color_;
  StyleColor border_right_color_;
  StyleColor border_top_color_;
  StyleColor border_bottom_color_;
  EBorderStyle border_left_style_;
  EBorderStyle border_right_style_;
  EBorderStyle border_top_style_;
  EBorderStyle border_bottom_style_;
  LayoutUnit border_left_width_;
  LayoutUnit border_right_width_;
  LayoutUnit border_top_width_;
  LayoutUnit border_bottom_width_;
  NinePieceImage border_image_;
  FillLayer background_layers_;
  StyleColor background_color_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_UA_STYLE_H_
