// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

ShelfControlButton::ShelfControlButton(
    Shelf* shelf,
    ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate) {
  set_has_ink_drop_action_on_click(true);
  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetColor(kShelfFocusBorderColor);
  SetFocusPainter(nullptr);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return gfx::Point(width() / 2.f, height() / 2.f);
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  const int button_radius = ShelfConstants::control_border_radius();
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - button_radius, center.y() - button_radius,
                   2 * button_radius, 2 * button_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConstants::control_border_radius());
}

const char* ShelfControlButton::GetClassName() const {
  return "ash/ShelfControlButton";
}

void ShelfControlButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ShelfButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, GetContentsBounds());
}

void ShelfControlButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const int border_radius = ShelfConstants::control_border_radius();
  // Some control buttons have a slightly larger size to fill the shelf and
  // maximize the click target, but we still want their "visual" size to be
  // the same, so we find the center point and draw a square around that.
  const gfx::Point center = GetCenterPoint();
  const int half_size = ShelfConstants::control_size() / 2;
  const gfx::Rect visual_size(center.x() - half_size, center.y() - half_size,
                              ShelfConstants::control_size(),
                              ShelfConstants::control_size());
  auto path = std::make_unique<SkPath>();
  path->addRoundRect(gfx::RectToSkRect(visual_size), border_radius,
                     border_radius);
  SetProperty(views::kHighlightPathKey, path.release());
  ShelfButton::OnBoundsChanged(previous_bounds);
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kShelfControlPermanentHighlightBackground);
  canvas->DrawRoundRect(bounds, ShelfConstants::control_border_radius(), flags);
}

}  // namespace ash
