// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

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
  params.name = view->GetClassName();

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->SetFullscreen(true);
}

}  // namespace

AmbientContainerView::AmbientContainerView(
    AmbientController* ambient_controller)
    : ambient_controller_(ambient_controller) {
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

void AmbientContainerView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    event->SetHandled();
    GetWidget()->Close();
  }
}

void AmbientContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    event->SetHandled();
    GetWidget()->Close();
  }
}

void AmbientContainerView::Init() {
  CreateWidget(this);
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));

  photo_view_ = new PhotoView(ambient_controller_);
  AddChildView(photo_view_);
}

}  // namespace ash
