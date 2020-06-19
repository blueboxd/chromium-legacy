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
#include "ash/public/cpp/ambient/ambient_ui_model.h"
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
constexpr int kAssistantPreferredHeightDip = 128;

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
  // Layout child views first to have proper bounds set for children.
  LayoutPhotoView();
  LayoutAssistantView();
  LayoutGlanceableInfoView();

  View::Layout();
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
}

void AmbientContainerView::LayoutPhotoView() {
  // |photo_view_| should have the same size as the widget.
  photo_view_->SetBoundsRect(GetLocalBounds());
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

void AmbientContainerView::LayoutAssistantView() {
  int preferred_width = GetPreferredSize().width();
  int preferred_height = kAssistantPreferredHeightDip;
  ambient_assistant_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, preferred_width, preferred_height));
}

}  // namespace ash
