// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_painter.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Placeholder colors and alphas. TODO(crbug.com/933384): get rid of
// these and make colors configurable, with same defaults as existing
// ink drops.
constexpr SkColor kInstallableInkDropBaseColor = SK_ColorBLACK;
constexpr SkAlpha kInstallableInkDropHighlightedOpacity = 0.08 * SK_AlphaOPAQUE;
constexpr SkAlpha kInstallableInkDropActivatedOpacity = 0.16 * SK_AlphaOPAQUE;
}  // namespace

namespace views {

gfx::Size InstallableInkDropPainter::GetMinimumSize() const {
  return gfx::Size();
}

void InstallableInkDropPainter::Paint(gfx::Canvas* canvas,
                                      const gfx::Size& size) {
  if (activated_) {
    canvas->FillRect(gfx::Rect(size),
                     SkColorSetA(kInstallableInkDropBaseColor,
                                 kInstallableInkDropActivatedOpacity));
  } else if (highlighted_) {
    canvas->FillRect(gfx::Rect(size),
                     SkColorSetA(kInstallableInkDropBaseColor,
                                 kInstallableInkDropHighlightedOpacity));
  }
}

}  // namespace views
