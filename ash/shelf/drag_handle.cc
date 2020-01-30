// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_handle.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/timer/timer.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

// Vertical padding to make the drag handle easier to tap.
constexpr int kVerticalClickboxPadding = 15;

}  // namespace

DragHandle::DragHandle(gfx::Size drag_handle_size,
                       AshColorProvider::RippleAttributes ripple_attributes,
                       int drag_handle_corner_radius) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetColor(ripple_attributes.base_color);
  // TODO(manucornet): Figure out why we need a manual opacity adjustment
  // to make this color look the same as the status area highlight.
  layer()->SetOpacity(ripple_attributes.inkdrop_opacity + 0.075);
  layer()->SetRoundedCornerRadius(
      {drag_handle_corner_radius, drag_handle_corner_radius,
       drag_handle_corner_radius, drag_handle_corner_radius});
  SetSize(drag_handle_size);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

DragHandle::~DragHandle() = default;

bool DragHandle::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect drag_handle_bounds = target->GetLocalBounds();
  drag_handle_bounds.set_y(drag_handle_bounds.y() - kVerticalClickboxPadding);
  drag_handle_bounds.set_height(drag_handle_bounds.height() +
                                2 * kVerticalClickboxPadding);
  return drag_handle_bounds.Intersects(rect);
}

}  // namespace ash
