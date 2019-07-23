// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_icon_view.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"
#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace {

// Height of the loader bar in DIP.
constexpr float kLoaderHeight = 4.0f;
// Width of the loader bar in percent of its range.
constexpr float kLoaderWidth = 0.2f;

// TODO(knollr): move these into IconLabelBubbleView.
constexpr int kIconTextSpacing = 8;
constexpr int kIconTextSpacingTouch = 10;

ClickToCallSharingDialogController* GetControllerFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return ClickToCallSharingDialogController::GetOrCreateFromWebContents(
      web_contents);
}

}  // namespace

ClickToCallIconView::ClickToCallIconView(PageActionIconView::Delegate* delegate)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         delegate) {
  SetVisible(false);
  UpdateLoaderColor();
  SetUpForInOutAnimation();
}

ClickToCallIconView::~ClickToCallIconView() = default;

views::BubbleDialogDelegateView* ClickToCallIconView::GetBubble() const {
  auto* controller = GetControllerFromWebContents(GetWebContents());
  return controller
             ? static_cast<ClickToCallDialogView*>(controller->GetDialog())
             : nullptr;
}

bool ClickToCallIconView::Update() {
  auto* controller = GetControllerFromWebContents(GetWebContents());
  if (!controller)
    return false;

  if (show_error_ != controller->send_failed()) {
    show_error_ = controller->send_failed();
    UpdateIconImage();
  }

  if (controller->is_loading())
    StartLoadingAnimation();
  else
    StopLoadingAnimation();

  const bool is_bubble_showing = IsBubbleShowing();

  if (is_bubble_showing || loading_animation_ || last_controller_ != controller)
    ResetSlideAnimation(/*show=*/false);

  last_controller_ = controller;

  const bool is_visible =
      is_bubble_showing || loading_animation_ || label()->GetVisible();
  const bool visibility_changed = GetVisible() != is_visible;

  SetVisible(is_visible);
  UpdateInkDrop(is_bubble_showing);
  return visibility_changed;
}

void ClickToCallIconView::StartLoadingAnimation() {
  if (loading_animation_)
    return;
  loading_animation_ = std::make_unique<gfx::ThrobAnimation>(this);
  loading_animation_->SetTweenType(gfx::Tween::LINEAR);
  loading_animation_->SetThrobDuration(750);
  loading_animation_->StartThrobbing(-1);
  SchedulePaint();
}

void ClickToCallIconView::StopLoadingAnimation() {
  if (!loading_animation_)
    return;

  if (!show_error_)
    AnimateIn(IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_SEND_SUCCESS);

  loading_animation_.reset();
  SchedulePaint();
}

void ClickToCallIconView::PaintButtonContents(gfx::Canvas* canvas) {
  PageActionIconView::PaintButtonContents(canvas);
  if (!loading_animation_)
    return;

  // TODO(knollr): Add support for this animation to PageActionIconView if other
  // features need it as well.

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  const gfx::Rect icon_bounds =
      gfx::ScaleToEnclosedRect(image()->bounds(), scale);
  const float progress = loading_animation_->GetCurrentValue();
  const float range = icon_bounds.width();
  const float offset = icon_bounds.x();

  // Calculate start and end in percent of range.
  float start = std::max(0.0f, (progress - kLoaderWidth) / (1 - kLoaderWidth));
  float end = std::min(1.0f, progress / (1 - kLoaderWidth));
  // Convert percentages to actual location.
  const float size = kLoaderHeight * scale;
  start = start * (range - size);
  end = end * (range - size) + size;

  gfx::RectF bounds(start + offset, icon_bounds.bottom() - size, end - start,
                    size);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(loader_color_);
  canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
}

double ClickToCallIconView::WidthMultiplier() const {
  double multiplier = PageActionIconView::WidthMultiplier();

  double min_width = image()->GetPreferredSize().width() + GetInsets().width();
  double spacing = ui::MaterialDesignController::touch_ui()
                       ? kIconTextSpacingTouch
                       : kIconTextSpacing;
  double label_width = label()->GetPreferredSize().width();
  double max_width = min_width + spacing + label_width;

  // We offset the width multiplier to start expanding the label straight away
  // instead of completely hide the icon and expanding it from zero width.
  double offset = min_width / max_width;
  return multiplier * (1 - offset) + offset;
}

void ClickToCallIconView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != loading_animation_.get()) {
    UpdateOpacity();
    return PageActionIconView::AnimationProgressed(animation);
  }
  SchedulePaint();
}

void ClickToCallIconView::AnimationEnded(const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);
  UpdateOpacity();
  Update();
}

void ClickToCallIconView::OnThemeChanged() {
  PageActionIconView::OnThemeChanged();
  UpdateLoaderColor();
}

void ClickToCallIconView::UpdateOpacity() {
  if (!IsShrinking()) {
    DestroyLayer();
    SetTextSubpixelRenderingEnabled(true);
    return;
  }

  if (!layer()) {
    SetPaintToLayer();
    SetTextSubpixelRenderingEnabled(false);
    layer()->SetFillsBoundsOpaquely(false);
  }

  layer()->SetOpacity(PageActionIconView::WidthMultiplier());
}

void ClickToCallIconView::UpdateInkDrop(bool activate) {
  auto target_state =
      activate ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN;
  if (GetInkDrop()->GetTargetInkDropState() != target_state)
    AnimateInkDrop(target_state, /*event=*/nullptr);
}

void ClickToCallIconView::UpdateLoaderColor() {
  loader_color_ = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
}

bool ClickToCallIconView::IsTriggerableEvent(const ui::Event& event) {
  // We do nothing when the icon is clicked.
  return false;
}

void ClickToCallIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& ClickToCallIconView::GetVectorIcon() const {
  return vector_icons::kCallIcon;
}

const gfx::VectorIcon& ClickToCallIconView::GetVectorIconBadge() const {
  return show_error_ ? kBlockedBadgeIcon : gfx::kNoneIcon;
}

base::string16 ClickToCallIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}
