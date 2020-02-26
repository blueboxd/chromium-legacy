// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_nudge.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Shelf item tooltip height.
constexpr int kTooltipHeight = 18;

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
constexpr int kTooltipMaxWidth = 250;

}  // namespace

ContextualNudge::ContextualNudge(views::View* anchor,
                                 const base::string16& text)
    : views::BubbleDialogDelegateView(anchor,
                                      views::BubbleBorder::BOTTOM_CENTER,
                                      views::BubbleBorder::NO_ASSETS) {
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets(0, 0));
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_adjust_if_offscreen(false);
  set_shadow(views::BubbleBorder::NO_ASSETS);
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_ShelfContainer));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = AddChildView(std::make_unique<views::Label>(text));
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetEnabledColor(SkColorSetA(gfx::kGoogleGrey200, 0xFF));
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Text box for shelf nudge should be ignored for collision detection.
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

ContextualNudge::~ContextualNudge() = default;

gfx::Size ContextualNudge::CalculatePreferredSize() const {
  const gfx::Size size = BubbleDialogDelegateView::CalculatePreferredSize();
  return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                   std::max(size.height(), kTooltipHeight));
}

ui::LayerType ContextualNudge::GetLayerType() const {
  return ui::LAYER_NOT_DRAWN;
}

}  // namespace ash
