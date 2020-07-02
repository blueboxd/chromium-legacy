// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)

namespace ash {

namespace {

constexpr base::TimeDelta kAutoShowWaitTimeInterval =
    base::TimeDelta::FromSeconds(15);

// Used by wake lock APIs.
constexpr char kWakeLockReason[] = "AmbientMode";

void CloseAssistantUi() {
  DCHECK(AssistantUiController::Get());
  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::AssistantExitPoint::kUnspecified);
}

std::unique_ptr<AmbientBackendController> CreateAmbientBackendController() {
#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
  return std::make_unique<AmbientBackendControllerImpl>();
#else
  return std::make_unique<FakeAmbientBackendControllerImpl>();
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
}

// Returns the parent container of ambient widget. Will return a nullptr for
// the in-session UI when lock-screen is currently not shown.
aura::Window* GetWidgetContainer() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock)) {
    return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                               kShellWindowId_LockScreenContainer);
  }
  return nullptr;
}

// Returns the name of the ambient widget.
std::string GetWidgetName() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock))
    return "LockScreenAmbientModeContainer";
  return "InSessionAmbientModeContainer";
}

// Returns true if the device is currently connected to a charger.
bool IsChargerConnected() {
  return (PowerStatus::Get()->IsBatteryCharging() ||
          PowerStatus::Get()->IsBatteryFull()) &&
         PowerStatus::Get()->IsLinePowerConnected();
}

}  // namespace

// AmbientController::InactivityMonitor----------------------------------

// Monitors the events when ambient screen is hidden, and shows up the screen
// automatically if the device has been inactive for a specific amount of time.
class AmbientController::InactivityMonitor : public ui::EventHandler {
 public:
  using AutoShowCallback = base::OnceCallback<void()>;

  InactivityMonitor(base::WeakPtr<views::Widget> target_widget,
                    AutoShowCallback callback)
      : target_widget_(target_widget) {
    timer_.Start(FROM_HERE, kAutoShowWaitTimeInterval, std::move(callback));

    DCHECK(target_widget_);
    target_widget_->GetNativeWindow()->AddPreTargetHandler(this);
  }

  ~InactivityMonitor() override {
    if (target_widget_) {
      target_widget_->GetNativeWindow()->RemovePreTargetHandler(this);
    }
  }

  InactivityMonitor(const InactivityMonitor&) = delete;
  InactivityMonitor& operator=(const InactivityMonitor&) = delete;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // Restarts the timer upon events from the target widget.
    timer_.Reset();
  }

 private:
  base::WeakPtr<views::Widget> target_widget_;
  // Will be canceled when out-of-scope.
  base::OneShotTimer timer_;
};

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

  ambient_ui_model_.AddObserver(this);
  // |SessionController| is initialized before |this| in Shell.
  Shell::Get()->session_controller()->AddObserver(this);
}

AmbientController::~AmbientController() {
  if (container_view_)
    container_view_->GetWidget()->CloseNow();

  // |SessionController| is destroyed after |this| in Shell.
  Shell::Get()->session_controller()->RemoveObserver(this);
  ambient_ui_model_.RemoveObserver(this);
}

void AmbientController::OnAmbientUiVisibilityChanged(
    AmbientUiVisibility visibility) {
  switch (visibility) {
    case AmbientUiVisibility::kShown:
      if (!container_view_)
        CreateWidget();
      else
        container_view_->GetWidget()->Show();

      if (inactivity_monitor_) {
        // Resets the monitor and cancels the timer upon shown.
        inactivity_monitor_.reset();
      }

      DCHECK(container_view_);
      // This will be no-op if the view is already visible.
      container_view_->SetVisible(true);

      if (IsChargerConnected()) {
        // Requires wake lock to prevent display from sleeping.
        AcquireWakeLock();
      }
      // Observes the |PowerStatus| on the battery charging status change for
      // the current ambient session.
      if (!power_status_observer_.IsObserving(PowerStatus::Get())) {
        power_status_observer_.Add(PowerStatus::Get());
      }

      StartRefreshingImages();
      break;
    case AmbientUiVisibility::kHidden:
      container_view_->GetWidget()->Hide();

      // Has no effect if |wake_lock_| has already been released.
      ReleaseWakeLock();
      StopRefreshingImages();

      // Creates the monitor and starts the auto-show timer upon hidden.
      DCHECK(!inactivity_monitor_);
      if (LockScreen::HasInstance()) {
        inactivity_monitor_ = std::make_unique<InactivityMonitor>(
            LockScreen::Get()->widget()->GetWeakPtr(),
            base::BindOnce(&AmbientController::OnAutoShowTimeOut,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case AmbientUiVisibility::kClosed:
      DCHECK(container_view_);
      // |CloseNow()| will close the widget synchronously to ensure its
      // view hierarchy being destroyed before |this|.
      container_view_->GetWidget()->CloseNow();

      StopRefreshingImages();
      CleanUpOnClosed();

      // We close the Assistant UI after ambient screen not being shown to sync
      // states to |AssistantUiController|. This will be a no-op if the
      // |kAmbientAssistant| feature is disabled, or the Assistant UI has
      // already been closed.
      CloseAssistantUi();
      break;
  }
}

void AmbientController::OnAutoShowTimeOut() {
  DCHECK_EQ(ambient_ui_model_.ui_visibility(), AmbientUiVisibility::kHidden);
  DCHECK(!container_view_->GetWidget()->IsVisible());

  // Show ambient screen after time out.
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
}

void AmbientController::OnLockStateChanged(bool locked) {
  if (!AmbientClient::Get()->IsAmbientModeAllowed())
    return;

  if (locked) {
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
    ambient_ui_model_.SetUiMode(AmbientUiMode::kLockScreenUi);
    ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
  } else {
    // Ambient screen will be destroyed along with the lock screen when user
    // logs in.
    ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
  }
}

void AmbientController::OnPowerStatusChanged() {
  if (ambient_ui_model_.ui_visibility() != AmbientUiVisibility::kShown) {
    // No action needed if ambient screen is not shown.
    return;
  }

  if (IsChargerConnected()) {
    AcquireWakeLock();
  } else {
    ReleaseWakeLock();
  }
}

void AmbientController::AddAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.AddObserver(observer);
}

void AmbientController::RemoveAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.RemoveObserver(observer);
}

std::unique_ptr<AmbientContainerView> AmbientController::CreateContainerView() {
  DCHECK(!container_view_);

  auto container = std::make_unique<AmbientContainerView>(&delegate_);
  container_view_ = container.get();
  return container;
}

void AmbientController::HideContainerView() {
  DCHECK(container_view_);

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
}

void AmbientController::ShowInSessionUI() {
  // TODO(meilinw): move the eligibility check to the idle entry point once
  // implemented: b/149246117.
  if (!AmbientClient::Get()->IsAmbientModeAllowed()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiMode(AmbientUiMode::kInSessionUi);
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
}

void AmbientController::CloseInSessionUI() {
  DCHECK(container_view_);

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
}

void AmbientController::ToggleInSessionUI() {
  if (!container_view_)
    ShowInSessionUI();
  else
    CloseInSessionUI();
}

bool AmbientController::IsShown() const {
  return container_view_ && container_view_->IsDrawn();
}

void AmbientController::OnBackgroundPhotoEvents() {
  // Dismisses the ambient screen when user interacts with the background photo.
  if (ambient_ui_model_.ui_mode() == AmbientUiMode::kInSessionUi)
    CloseInSessionUI();
  else
    HideContainerView();
}

void AmbientController::UpdateUiMode(AmbientUiMode ui_mode) {
  ambient_ui_model_.SetUiMode(ui_mode);
}

void AmbientController::AcquireWakeLock() {
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> provider;
    AmbientClient::Get()->RequestWakeLockProvider(
        provider.BindNewPipeAndPassReceiver());
    provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, kWakeLockReason,
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  DCHECK(wake_lock_);
  wake_lock_->RequestWakeLock();
  VLOG(1) << "Acquired wake lock";
}

void AmbientController::ReleaseWakeLock() {
  if (!wake_lock_)
    return;

  wake_lock_->CancelWakeLock();
  VLOG(1) << "Released wake lock";
}

void AmbientController::RequestAccessToken(
    AmbientAccessTokenController::AccessTokenCallback callback) {
  access_token_controller_.RequestAccessToken(std::move(callback));
}

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  return ambient_photo_controller_.ambient_backend_model();
}

void AmbientController::CreateWidget() {
  DCHECK(!container_view_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = GetWidgetName();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.parent = GetWidgetContainer();

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->SetContentsView(CreateContainerView());

  widget->Show();

  // Requests keyboard focus for |container_view_| to receive keyboard events.
  container_view_->RequestFocus();
}

void AmbientController::CleanUpOnClosed() {
  // Invalidates the view pointer.
  container_view_ = nullptr;
  inactivity_monitor_.reset();
  power_status_observer_.Remove(PowerStatus::Get());

  // Should do nothing if the wake lock has already been released.
  ReleaseWakeLock();
}

void AmbientController::StartRefreshingImages() {
  ambient_photo_controller_.StartScreenUpdate();
}

void AmbientController::StopRefreshingImages() {
  ambient_photo_controller_.StopScreenUpdate();
}

void AmbientController::set_backend_controller_for_testing(
    std::unique_ptr<AmbientBackendController> backend_controller) {
  ambient_backend_controller_ = std::move(backend_controller);
}

}  // namespace ash
