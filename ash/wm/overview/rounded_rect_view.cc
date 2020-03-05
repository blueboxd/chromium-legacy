// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/rounded_rect_view.h"

#include "ui/gfx/canvas.h"

namespace ash {

RoundedRectView::RoundedRectView(int corner_radius, SkColor background_color)
    : corner_radius_(corner_radius), background_color_(background_color) {}

RoundedRectView::~RoundedRectView() = default;

void RoundedRectView::SetCornerRadius(int radius) {
  if (corner_radius_ == radius)
    return;

  corner_radius_ = radius;
  SchedulePaint();
}

void RoundedRectView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(background_color_);
  flags.setAntiAlias(true);

  gfx::RectF bounds(GetLocalBounds());
  canvas->DrawRoundRect(bounds, corner_radius_, flags);
}

}  // namespace ash
