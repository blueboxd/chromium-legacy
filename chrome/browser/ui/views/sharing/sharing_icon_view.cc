// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/ink_drop.h"

namespace {
// Height of the loader bar in DIP.
constexpr float kLoaderHeight = 4.0f;
// Width of the loader bar in percent of its range.
constexpr float kLoaderWidth = 0.2f;

// TODO(knollr): move these into IconLabelBubbleView.
constexpr int kIconTextSpacing = 8;
constexpr int kIconTextSpacingTouch = 10;
}  // namespace

SharingIconView::SharingIconView(PageActionIconView::Delegate* delegate)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         delegate) {
  SetVisible(false);
  UpdateLoaderColor();
  SetUpForInOutAnimation();
}

SharingIconView::~SharingIconView() = default;

void SharingIconView::StartLoadingAnimation() {
  if (loading_animation_)
    return;
  loading_animation_ = std::make_unique<gfx::ThrobAnimation>(this);
  loading_animation_->SetTweenType(gfx::Tween::LINEAR);
  loading_animation_->SetThrobDuration(750);
  loading_animation_->StartThrobbing(-1);
  SchedulePaint();
}

void SharingIconView::StopLoadingAnimation(base::Optional<int> string_id) {
  if (!loading_animation_)
    return;

  if (!should_show_error_)
    AnimateIn(string_id);

  loading_animation_.reset();
  SchedulePaint();
}

void SharingIconView::UpdateLoaderColor() {
  loader_color_ = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
}

void SharingIconView::OnThemeChanged() {
  PageActionIconView::OnThemeChanged();
  UpdateLoaderColor();
}

void SharingIconView::PaintButtonContents(gfx::Canvas* canvas) {
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

double SharingIconView::WidthMultiplier() const {
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

void SharingIconView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != loading_animation_.get()) {
    UpdateOpacity();
    return PageActionIconView::AnimationProgressed(animation);
  }
  SchedulePaint();
}

void SharingIconView::AnimationEnded(const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);
  UpdateOpacity();
  Update();
}

void SharingIconView::UpdateOpacity() {
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

void SharingIconView::UpdateInkDrop(bool activate) {
  auto target_state =
      activate ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN;
  if (GetInkDrop()->GetTargetInkDropState() != target_state)
    AnimateInkDrop(target_state, /*event=*/nullptr);
}

bool SharingIconView::IsTriggerableEvent(const ui::Event& event) {
  // We do nothing when the icon is clicked.
  return false;
}

const gfx::VectorIcon& SharingIconView::GetVectorIconBadge() const {
  return should_show_error_ ? kBlockedBadgeIcon : gfx::kNoneIcon;
}

void SharingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

bool SharingIconView::isLoadingAnimationVisible() {
  return loading_animation_.get();
}
