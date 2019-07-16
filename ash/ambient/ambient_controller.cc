// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include "ash/ambient/model/photo_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool CanStartAmbientMode() {
  return chromeos::switches::IsAmbientModeEnabled() && PhotoController::Get() &&
         !ambient::util::IsShowing(LockScreen::ScreenType::kLogin);
}

}  // namespace

AmbientController::AmbientController() = default;

AmbientController::~AmbientController() {
  DestroyContainerView();
}

void AmbientController::OnWidgetDestroying(views::Widget* widget) {
  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;
}

void AmbientController::Toggle() {
  if (container_view_)
    Stop();
  else
    Start();
}

void AmbientController::AddPhotoModelObserver(PhotoModelObserver* observer) {
  model_.AddObserver(observer);
}

void AmbientController::RemovePhotoModelObserver(PhotoModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AmbientController::Start() {
  if (!CanStartAmbientMode()) {
    // TODO(wutao): Show a toast to indicate that Ambient mode is not ready.
    return;
  }

  CreateContainerView();
  container_view_->GetWidget()->Show();
  RefreshImage();
}

void AmbientController::Stop() {
  refresh_timer_.Stop();
  DestroyContainerView();
}

void AmbientController::CreateContainerView() {
  container_view_ = new AmbientContainerView(this);
  container_view_->GetWidget()->AddObserver(this);
}

void AmbientController::DestroyContainerView() {
  // |container_view_|'s widget is owned by its native widget. After calling
  // CloseNow(), it will trigger |OnWidgetDestroying|, where it will set the
  // |container_view_| to nullptr.
  if (container_view_)
    container_view_->GetWidget()->CloseNow();
}

void AmbientController::RefreshImage() {
  if (!PhotoController::Get())
    return;

  PhotoController::Get()->GetNextImage(base::BindOnce(
      &AmbientController::OnPhotoDownloaded, weak_factory_.GetWeakPtr()));

  constexpr base::TimeDelta kTimeOut = base::TimeDelta::FromMilliseconds(1000);
  refresh_timer_.Start(
      FROM_HERE, kTimeOut,
      base::BindOnce(&AmbientController::RefreshImage, base::Unretained(this)));
}

void AmbientController::OnPhotoDownloaded(const gfx::ImageSkia& image) {
  model_.AddNextImage(image);
}

AmbientContainerView* AmbientController::GetAmbientContainerViewForTesting() {
  return container_view_;
}

}  // namespace ash
