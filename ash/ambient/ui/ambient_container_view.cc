// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ui/ambient_assistant_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Appearance.
constexpr int kHorizontalMarginDip = 16;
constexpr int kVerticalMarginDip = 64;
constexpr int kAssistantContainerViewPreferredHeightDip = 128;

// TODO(meilinw): temporary values for dev purpose, need to be updated with the
// final spec.
constexpr float kBackgroundPhotoOpacity = 0.5f;
constexpr base::TimeDelta kBackgroundPhotoFadeOutAnimationDuration =
    base::TimeDelta::FromMilliseconds(500);

aura::Window* GetContainer() {
  aura::Window* container = nullptr;
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock))
    container = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                    kShellWindowId_LockScreenContainer);

  return container;
}

void CreateWidget(AmbientContainerView* view) {
  views::Widget::InitParams params;
  params.parent = GetContainer();
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.delegate = view;
  params.name = "AmbientModeContainer";

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->SetFullscreen(true);
}

}  // namespace

AmbientContainerView::AmbientContainerView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  Init();
}

AmbientContainerView::~AmbientContainerView() = default;

const char* AmbientContainerView::GetClassName() const {
  return "AmbientContainerView";
}

gfx::Size AmbientContainerView::CalculatePreferredSize() const {
  // TODO(b/139953389): Handle multiple displays.
  return GetWidget()->GetNativeWindow()->GetRootWindow()->bounds().size();
}

void AmbientContainerView::Layout() {
  if (photo_view_)
    photo_view_->SetBoundsRect(GetLocalBounds());

  if (ambient_assistant_container_view_) {
    // The view has the same width as the container view and the widget.
    int width = GetLocalBounds().width();
    ambient_assistant_container_view_->SetBounds(
        0, 0, width, kAssistantContainerViewPreferredHeightDip);
  }

  if (glanceable_info_view_)
    LayoutGlanceableInfoView();
}

void AmbientContainerView::FadeOutPhotoView() {
  DCHECK(photo_view_);

  photo_view_->layer()->GetAnimator()->StartAnimation(
      assistant::util::CreateLayerAnimationSequence(
          assistant::util::CreateOpacityElement(
              kBackgroundPhotoOpacity,
              kBackgroundPhotoFadeOutAnimationDuration)));
}

void AmbientContainerView::Init() {
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));

  photo_view_ = AddChildView(std::make_unique<PhotoView>(delegate_));

  ambient_assistant_container_view_ =
      AddChildView(std::make_unique<AmbientAssistantContainerView>());
  ambient_assistant_container_view_->SetVisible(false);

  glanceable_info_view_ =
      AddChildView(std::make_unique<GlanceableInfoView>(delegate_));

  CreateWidget(this);
}

void AmbientContainerView::LayoutGlanceableInfoView() {
  const gfx::Size container_size = GetLocalBounds().size();
  const gfx::Size preferred_size = glanceable_info_view_->GetPreferredSize();

  // The clock and weather view is positioned on the left-bottom corner of the
  // container.
  int x = kHorizontalMarginDip;
  int y =
      container_size.height() - kVerticalMarginDip - preferred_size.height();
  glanceable_info_view_->SetBoundsRect(
      gfx::Rect(x, y, preferred_size.width(), preferred_size.height()));
}

}  // namespace ash
