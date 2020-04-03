// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <string>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/photo_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_mode_state.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool CanStartAmbientMode() {
  return chromeos::features::IsAmbientModeEnabled() && PhotoController::Get() &&
         !ambient::util::IsShowing(LockScreen::ScreenType::kLogin);
}

void CloseAssistantUi() {
  auto* assistant_controller = Shell::Get()->assistant_controller();
  // |AssistantController| is initiated before the |AmbientController| in shell.
  DCHECK(assistant_controller);

  assistant_controller->ui_controller()->CloseUi(
      chromeos::assistant::mojom::AssistantExitPoint::kUnspecified);
}

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (chromeos::features::IsAmbientModeEnabled()) {
    registry->RegisterStringPref(ash::ambient::prefs::kAmbientBackdropClientId,
                                 std::string());

    // Do not sync across devices to allow different usages for different
    // devices.
    registry->RegisterBooleanPref(ash::ambient::prefs::kAmbientModeEnabled,
                                  true);
    registry->RegisterIntegerPref(
        ash::ambient::prefs::kAmbientModeTopicSource,
        static_cast<int>(ash::ambient::prefs::TopicSource::kArtGallery));
  }
}

AmbientController::AmbientController() {
  ambient_state_.AddObserver(this);
  // |SessionController| is initialized before |this| in Shell.
  Shell::Get()->session_controller()->AddObserver(this);
}

AmbientController::~AmbientController() {
  // |SessionController| is destroyed after |this| in Shell.
  Shell::Get()->session_controller()->RemoveObserver(this);
  ambient_state_.RemoveObserver(this);

  if (container_view_)
    DestroyContainerView();
}

void AmbientController::OnWidgetDestroying(views::Widget* widget) {
  refresh_timer_.Stop();
  photo_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;

  // Call CloseUi() explicitly to sync states to |AssistantUiController|.
  // This is a no-op if the UI has already been closed before the widget gets
  // destroyed.
  CloseAssistantUi();
}

void AmbientController::OnAmbientModeEnabled(bool enabled) {
  if (enabled) {
    CreateContainerView();
    container_view_->GetWidget()->Show();
    RefreshImage();
  } else {
    DestroyContainerView();
  }
}

void AmbientController::OnLockStateChanged(bool locked) {
  if (!locked) {
    // We should already exit ambient mode at this time, as the ambient
    // container needs to be closed to uncover the login port for
    // re-authentication.
    DCHECK(!container_view_);
    return;
  }

  // Show the ambient container on top of the lock screen.
  DCHECK(!container_view_);
  Start();
}

void AmbientController::Start() {
  if (!CanStartAmbientMode()) {
    // TODO(wutao): Show a toast to indicate that Ambient mode is not ready.
    return;
  }

  // CloseUi to ensure the embedded Assistant UI doesn't exist when entering
  // Ambient mode to avoid strange behavior caused by the embedded UI was
  // only hidden at that time. This will be a no-op if UI was already closed.
  // TODO(meilinw): Handle embedded UI.
  CloseAssistantUi();

  ambient_state_.SetAmbientModeEnabled(true);
}

void AmbientController::Stop() {
  ambient_state_.SetAmbientModeEnabled(false);
}
void AmbientController::Toggle() {
  if (container_view_)
    Stop();
  else
    Start();
}

void AmbientController::CreateContainerView() {
  DCHECK(!container_view_);
  container_view_ = new AmbientContainerView(&delegate_);
  container_view_->GetWidget()->AddObserver(this);
}

void AmbientController::DestroyContainerView() {
  // |container_view_|'s widget is owned by its native widget. After calling
  // |CloseNow|, |OnWidgetDestroying| will be triggered immediately to reset
  // |container_view_| to nullptr.
  if (container_view_)
    container_view_->GetWidget()->CloseNow();
}

void AmbientController::RefreshImage() {
  if (!PhotoController::Get())
    return;

  if (photo_model_.ShouldFetchImmediately()) {
    // TODO(b/140032139): Defer downloading image if it is animating.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientController::GetNextImage,
                       weak_factory_.GetWeakPtr()),
        kAnimationDuration);
  } else {
    photo_model_.ShowNextImage();
    ScheduleRefreshImage();
  }
}

void AmbientController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!photo_model_.ShouldFetchImmediately()) {
    // TODO(b/139953713): Change to a correct time interval.
    refresh_interval = base::TimeDelta::FromSeconds(5);
  }

  refresh_timer_.Start(FROM_HERE, refresh_interval,
                       base::BindOnce(&AmbientController::RefreshImage,
                                      weak_factory_.GetWeakPtr()));
}

void AmbientController::GetNextImage() {
  PhotoController::Get()->GetNextImage(base::BindOnce(
      &AmbientController::OnPhotoDownloaded, weak_factory_.GetWeakPtr()));
}

void AmbientController::OnPhotoDownloaded(bool success,
                                          const gfx::ImageSkia& image) {
  // TODO(b/148485116): Implement retry logic.
  if (!success)
    return;

  DCHECK(!image.isNull());
  photo_model_.AddNextImage(image);
  ScheduleRefreshImage();
}

}  // namespace ash
