// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_layer.h"

#include "ash/shell.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const int kDefaultStrokeWidth = 6;

display::Display GetPrimaryDisplay() {
  DCHECK(display::Screen::GetScreen());
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}
}  // namespace

PointScanLayer::PointScanLayer(AccessibilityLayerDelegate* delegate)
    : AccessibilityLayer(delegate) {
  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(GetPrimaryDisplay().id());
  CreateOrUpdateLayer(root_window, "PointScanning", gfx::Rect());
  SetOpacity(1.0);
}

void PointScanLayer::StartHorizontalScanning() {
  bounds_ = GetPrimaryDisplay().bounds();
  layer()->SetBounds(bounds_);

  gfx::Point start = bounds_.top_center();
  gfx::Point end = bounds_.bottom_center();
  horizontal_.start = start;
  horizontal_.end = end;
}

bool PointScanLayer::CanAnimate() const {
  return true;
}
bool PointScanLayer::NeedToAnimate() const {
  return true;
}
int PointScanLayer::GetInset() const {
  return 0;
}

void PointScanLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kDefaultStrokeWidth);
  flags.setColor(gfx::kGoogleBlue300);

  SkPath path;
  path.moveTo(horizontal_.start.x(), horizontal_.start.y());
  path.lineTo(horizontal_.end.x(), horizontal_.end.y());
  recorder.canvas()->DrawPath(path, flags);
}

}  // namespace ash
