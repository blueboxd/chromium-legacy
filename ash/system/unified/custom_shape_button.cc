// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/custom_shape_button.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"

namespace ash {

namespace {

// Ink drop mask that masks non-standard shape of CustomShapeButton.
class CustomShapeInkDropMask : public views::InkDropMask {
 public:
  CustomShapeInkDropMask(const gfx::Size& layer_size,
                         const CustomShapeButton* button);

 private:
  // InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  const CustomShapeButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(CustomShapeInkDropMask);
};

CustomShapeInkDropMask::CustomShapeInkDropMask(const gfx::Size& layer_size,
                                               const CustomShapeButton* button)
    : views::InkDropMask(layer_size), button_(button) {}

void CustomShapeInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setAlpha(255);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawPath(button_->CreateCustomShapePath(layer()->bounds()),
                              flags);
}

}  // namespace

CustomShapeButton::CustomShapeButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  TrayPopupUtils::ConfigureTrayPopupButton(this);
}

CustomShapeButton::~CustomShapeButton() = default;

void CustomShapeButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);
  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> CustomShapeButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> CustomShapeButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(), kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropHighlight>
CustomShapeButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this, kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropMask> CustomShapeButton::CreateInkDropMask()
    const {
  return std::make_unique<CustomShapeInkDropMask>(size(), this);
}

const char* CustomShapeButton::GetClassName() const {
  return "CustomShapeButton";
}

void CustomShapeButton::PaintCustomShapePath(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetEnabled() ? kUnifiedMenuButtonColor
                              : kUnifiedMenuButtonColorDisabled);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->DrawPath(CreateCustomShapePath(GetLocalBounds()), flags);
}

}  // namespace ash
