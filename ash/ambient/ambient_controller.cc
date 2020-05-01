// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_mode_state.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind_helpers.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)

namespace ash {

namespace {

bool CanStartAmbientMode() {
  return chromeos::features::IsAmbientModeEnabled() &&
         AmbientClient::Get()->IsAmbientModeAllowedForActiveUser();
}

void CloseAssistantUi() {
  DCHECK(AssistantUiController::Get());
  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::mojom::AssistantExitPoint::kUnspecified);
}

std::unique_ptr<AmbientBackendController> CreateAmbientBackendController() {
#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
  return std::make_unique<AmbientBackendControllerImpl>();
#else
  return std::make_unique<FakeAmbientBackendControllerImpl>();
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
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
  }
}

AmbientController::AmbientController() {
  ambient_backend_controller_ = CreateAmbientBackendController();
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
  ambient_backend_model_.Clear();
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
  if (locked) {
    // Show ambient mode when entering lock screen.
    DCHECK(!container_view_);

    // We have 3 options to manage the token for lock screen. Here use option 1.
    // 1. Request only one time after entering lock screen. We will use it once
    //    to request all the image links and no more requests.
    // 2. Request one time before entering lock screen. This will introduce
    //    extra latency.
    // 3. Request and refresh the token in the background (even the ambient mode
    //    is not started) with extra buffer time to use. When entering
    //    lock screen, it will be most likely to have the token already and
    //    enough time to use. More specifically,
    //    3a. We will leave enough buffer time (e.g. 10 mins before expire) to
    //        start to refresh the token.
    //    3b. When lock screen is triggered, most likely we will have >10 mins
    //        of token which can be used on lock screen.
    //    3c. There is a corner case that we may not have the token fetched when
    //        locking screen, we probably can use PrepareForLock(callback) when
    //        locking screen. We can add the refresh token into it. If the token
    //        has already been fetched, then there is not additional time to
    //        wait.
    RequestAccessToken(base::DoNothing());

    Show();
  } else {
    // Destroy ambient mode after user re-login.
    Destroy();
  }
}

void AmbientController::Show() {
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

void AmbientController::Destroy() {
  ambient_state_.SetAmbientModeEnabled(false);
}

void AmbientController::Toggle() {
  if (container_view_)
    Destroy();
  else
    Show();
}

void AmbientController::OnBackgroundPhotoEvents() {
  refresh_timer_.Stop();

  // Move the |AmbientModeContainer| beneath the |LockScreenWidget| to show
  // the lock screen contents on top before the fade-out animation.
  auto* ambient_window = container_view_->GetWidget()->GetNativeWindow();
  ambient_window->parent()->StackChildAtBottom(ambient_window);

  // Start fading out the current background photo.
  StartFadeOutAnimation();
}

void AmbientController::StartFadeOutAnimation() {
  // We fade out the |PhotoView| on its own layer instead of using the general
  // layer of the widget, otherwise it will reveal the color of the lockscreen
  // wallpaper beneath.
  container_view_->FadeOutPhotoView();
}

void AmbientController::RequestAccessToken(
    AmbientAccessTokenController::AccessTokenCallback callback) {
  access_token_controller_.RequestAccessToken(std::move(callback));
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
  if (ambient_backend_model_.ShouldFetchImmediately()) {
    // TODO(b/140032139): Defer downloading image if it is animating.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientController::GetNextImage,
                       weak_factory_.GetWeakPtr()),
        kAnimationDuration);
  } else {
    ambient_backend_model_.ShowNextImage();
    ScheduleRefreshImage();
  }
}

void AmbientController::ScheduleRefreshImage() {
  const base::TimeDelta refresh_interval =
      ambient_backend_model_.GetPhotoRefreshInterval();
  refresh_timer_.Start(FROM_HERE, refresh_interval,
                       base::BindOnce(&AmbientController::RefreshImage,
                                      weak_factory_.GetWeakPtr()));
}

void AmbientController::GetNextImage() {
  // Start requesting photo and weather information from the backdrop server.
  ambient_photo_controller_.GetNextScreenUpdateInfo(
      base::BindOnce(&AmbientController::OnPhotoDownloaded,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&AmbientController::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr()));
}

void AmbientController::OnPhotoDownloaded(const gfx::ImageSkia& image) {
  // TODO(b/148485116): Implement retry logic.
  if (image.isNull())
    return;

  ambient_backend_model_.AddNextImage(image);
  ScheduleRefreshImage();
}

void AmbientController::OnWeatherConditionIconDownloaded(
    base::Optional<float> temp_f,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull() || !temp_f.has_value())
    return;

  ambient_backend_model_.UpdateWeatherInfo(icon, temp_f.value());
}

void AmbientController::set_backend_controller_for_testing(
    std::unique_ptr<AmbientBackendController> backend_controller) {
  ambient_backend_controller_ = std::move(backend_controller);
}

}  // namespace ash
