// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_

#include "ui/views/painter.h"

namespace views {

// Holds the current visual state of the installable ink drop and handles
// painting it. The |Painter::Paint()| implementation draws a rectangular ink
// drop of the given size; the user should set a clip path via
// |gfx::Canvas::ClipPath()| to control the shape.
class VIEWS_EXPORT InstallableInkDropPainter : public Painter {
 public:
  InstallableInkDropPainter() = default;
  ~InstallableInkDropPainter() override = default;

  void set_activated(bool activated) { activated_ = activated; }
  bool activated() const { return activated_; }

  void set_highlighted_ratio(float highlighted_ratio) {
    highlighted_ratio_ = highlighted_ratio;
  }
  bool highlighted_ratio() const { return highlighted_ratio_; }

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  float highlighted_ratio_ = 0.0f;
  bool activated_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_
