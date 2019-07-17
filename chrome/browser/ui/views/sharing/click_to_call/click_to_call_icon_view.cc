// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_icon_view.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace {
// Height of the loader bar in DIP.
constexpr float kLoaderHeight = 4.0f;
// Width of the loader bar in percent of its range.
constexpr float kLoaderWidth = 0.2f;
}  // namespace

ClickToCallIconView::ClickToCallIconView(PageActionIconView::Delegate* delegate)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         delegate) {
  SetVisible(false);
  UpdateLoaderColor();
}

ClickToCallIconView::~ClickToCallIconView() = default;

views::BubbleDialogDelegateView* ClickToCallIconView::GetBubble() const {
  return ClickToCallDialogView::GetBubbleView();
}

bool ClickToCallIconView::Update() {
  const bool is_bubble_showing = IsBubbleShowing();
  const bool is_visible = is_bubble_showing || loading_animation_;
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
  start = start * (range - kLoaderHeight);
  end = end * (range - kLoaderHeight) + kLoaderHeight;

  gfx::RectF bounds(start + offset, icon_bounds.bottom() - kLoaderHeight,
                    end - start, kLoaderHeight);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(loader_color_);
  canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
}

void ClickToCallIconView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != loading_animation_.get())
    return PageActionIconView::AnimationProgressed(animation);
  SchedulePaint();
}

void ClickToCallIconView::OnThemeChanged() {
  PageActionIconView::OnThemeChanged();
  UpdateLoaderColor();
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
  // TODO(crbug.com/972059): Should we have our own icon?
  return kSendTabToSelfIcon;
}

base::string16 ClickToCallIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}
