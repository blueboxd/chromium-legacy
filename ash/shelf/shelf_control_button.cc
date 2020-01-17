// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ShelfControlButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  ShelfControlButtonHighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    const int border_radius = ShelfConfig::Get()->control_border_radius();
    // Some control buttons have a slightly larger size to fill the shelf and
    // maximize the click target, but we still want their "visual" size to be
    // the same, so we find the center point and draw a square around that.
    const gfx::Point center = view->GetLocalBounds().CenterPoint();
    const int half_size = ShelfConfig::Get()->control_size() / 2;
    gfx::Rect visual_size(center.x() - half_size, center.y() - half_size,
                          ShelfConfig::Get()->control_size(),
                          ShelfConfig::Get()->control_size());
    if (chromeos::switches::ShouldShowShelfHotseat() &&
        Shell::Get()->tablet_mode_controller()->InTabletMode() &&
        ShelfConfig::Get()->is_in_app()) {
      visual_size.Inset(
          0, ShelfConfig::Get()->in_app_control_button_height_inset());
    }
    return SkPath().addRoundRect(gfx::RectToSkRect(visual_size), border_radius,
                                 border_radius);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfControlButtonHighlightPathGenerator);
};

}  // namespace

ShelfControlButton::ShelfControlButton(
    Shelf* shelf,
    ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate) {
  set_has_ink_drop_action_on_click(true);
  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ShelfControlButtonHighlightPathGenerator>());
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  SetFocusPainter(nullptr);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return GetLocalBounds().CenterPoint();
}

void ShelfControlButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  int radius = ShelfConfig::Get()->control_border_radius();

  ink_drop_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(radius));
  ink_drop_layer->SetIsFastRoundedCorner(true);

  if (chromeos::switches::ShouldShowShelfHotseat() &&
      Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      ShelfConfig::Get()->is_in_app()) {
    // Control button highlights are oval in in-app, so adjust the insets.
    gfx::Rect clip(size());
    clip.Inset(0, ShelfConfig::Get()->in_app_control_button_height_inset());
    ink_drop_layer->SetClipRect(clip);
  } else {
    gfx::Point center = GetCenterPoint();
    gfx::Rect clip(center.x(), center.y(), 0, 0);
    clip.Inset(-radius, -radius);
    ink_drop_layer->SetClipRect(clip);
  }

  ShelfButton::AddInkDropLayer(ink_drop_layer);
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  // The mask is either cicle or rounded rects. Just use layer's RoundedCorner
  // api which is faster.
  return nullptr;
}

const char* ShelfControlButton::GetClassName() const {
  return "ash/ShelfControlButton";
}

gfx::Size ShelfControlButton::CalculatePreferredSize() const {
  return gfx::Size(ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

void ShelfControlButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ShelfButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, GetContentsBounds());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      ShelfConfig::Get()->shelf_control_permanent_highlight_background());
  canvas->DrawRoundRect(bounds, ShelfConfig::Get()->control_border_radius(),
                        flags);
}

}  // namespace ash
