// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "update-reject-cellular";

#if !defined(OFFICIAL_BUILD)
constexpr const char kUserActionCancelUpdateShortcut[] = "cancel-update";
#endif

// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

// Progress bar stages. Each represents progress bar value
// at the beginning of each stage.
// TODO(nkostylev): Base stage progress values on approximate time.
// TODO(nkostylev): Animate progress during each state.
const int kBeforeUpdateCheckProgress = 7;
const int kBeforeDownloadProgress = 14;
const int kBeforeVerifyingProgress = 74;
const int kBeforeFinalizingProgress = 81;
const int kProgressComplete = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

// Minimum timestep between two consecutive measurements for the download rates.
const int kMinTimeStepInSeconds = 1;

// Smooth factor that is used for the average downloading speed
// estimation.
// avg_speed = smooth_factor * cur_speed + (1.0 - smooth_factor) *
// avg_speed.
const double kDownloadSpeedSmoothFactor = 0.1;

// Minumum allowed value for the average downloading speed.
const double kDownloadAverageSpeedDropBound = 1e-8;

// An upper bound for possible downloading time left estimations.
const double kMaxTimeLeft = 24 * 60 * 60;

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
const int kDelayErrorMessageSec = 10;

const int kShowDelayMs = 400;

}  // anonymous namespace

// static
UpdateScreen* UpdateScreen::Get(ScreenManager* manager) {
  return static_cast<UpdateScreen*>(manager->GetScreen(UpdateView::kScreenId));
}

UpdateScreen::UpdateScreen(UpdateView* view,
                           ErrorScreen* error_screen,
                           const ScreenExitCallback& exit_callback)
    : BaseScreen(UpdateView::kScreenId),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      reboot_check_delay_(kWaitForRebootTimeSec),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(new ErrorScreensHistogramHelper("Update")) {
  if (view_)
    view_->Bind(this);
}

UpdateScreen::~UpdateScreen() {
  if (view_)
    view_->Unbind();

  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  network_portal_detector::GetInstance()->RemoveObserver(this);
}

void UpdateScreen::OnViewDestroyed(UpdateView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UpdateScreen::StartNetworkCheck() {
  // If portal detector is enabled and portal detection before AU is
  // allowed, initiate network state check. Otherwise, directly
  // proceed to update.
  if (!network_portal_detector::GetInstance()->IsEnabled()) {
    StartUpdateCheck();
    return;
  }
  state_ = State::STATE_FIRST_PORTAL_CHECK;
  is_first_detection_notification_ = true;
  is_first_portal_notification_ = true;
  network_portal_detector::GetInstance()->AddAndFireObserver(this);
}

void UpdateScreen::SetIgnoreIdleStatus(bool ignore_idle_status) {
  ignore_idle_status_ = ignore_idle_status;
}

void UpdateScreen::ExitUpdate(Result result) {
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  network_portal_detector::GetInstance()->RemoveObserver(this);
  show_timer_.Stop();

  exit_callback_.Run(result);
}

void UpdateScreen::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  if (is_checking_for_update_ &&
      status.status > UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE &&
      status.status != UpdateEngineClient::UPDATE_STATUS_ERROR &&
      status.status !=
          UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    is_checking_for_update_ = false;
  }
  if (ignore_idle_status_ &&
      status.status > UpdateEngineClient::UPDATE_STATUS_IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.status) {
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      MakeSureScreenIsShown();
      if (view_) {
        view_->SetProgress(kBeforeDownloadProgress);
        view_->SetShowEstimatedTimeLeft(false);
      }
      if (!HasCriticalUpdate()) {
        VLOG(1) << "Noncritical update available: " << status.new_version;
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      } else {
        VLOG(1) << "Critical update available: " << status.new_version;
        if (view_) {
          view_->SetProgressMessage(
              l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE));
          view_->SetShowCurtain(false);
        }
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      MakeSureScreenIsShown();
      if (!is_downloading_update_) {
        // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE
        // we need to is update critical on first downloading notification.
        is_downloading_update_ = true;
        download_start_time_ = download_last_time_ = tick_clock_->NowTicks();
        download_start_progress_ = status.download_progress;
        download_last_progress_ = status.download_progress;
        is_download_average_speed_computed_ = false;
        download_average_speed_ = 0.0;
        if (!HasCriticalUpdate()) {
          VLOG(1) << "Non-critical update available: " << status.new_version;
          ExitUpdate(Result::UPDATE_NOT_REQUIRED);
        } else {
          VLOG(1) << "Critical update available: " << status.new_version;
          if (view_) {
            view_->SetProgressMessage(
                l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE));
            view_->SetShowCurtain(false);
          }
        }
      }
      UpdateDownloadingStats(status);
      break;
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      MakeSureScreenIsShown();
      if (view_) {
        view_->SetProgress(kBeforeVerifyingProgress);
        view_->SetProgressMessage(
            l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING));
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      MakeSureScreenIsShown();
      if (view_) {
        view_->SetProgress(kBeforeFinalizingProgress);
        view_->SetProgressMessage(
            l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING));
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      if (view_) {
        view_->SetProgress(kProgressComplete);
        view_->SetShowEstimatedTimeLeft(false);
      }
      if (HasCriticalUpdate()) {
        if (view_)
          view_->SetShowCurtain(false);
        VLOG(1) << "Initiate reboot after update";
        DBusThreadManager::Get()->GetUpdateEngineClient()->RebootAfterUpdate();
        reboot_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(reboot_check_delay_),
                            this, &UpdateScreen::OnWaitForRebootTimeElapsed);
      } else {
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE:
      VLOG(1) << "Update requires user permission to proceed.";
      state_ = State::STATE_REQUESTING_USER_PERMISSION;
      pending_update_version_ = status.new_version;
      pending_update_size_ = status.new_size;

      DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);

      MakeSureScreenIsShown();
      if (view_) {
        view_->SetRequiresPermissionForCellular(true);
        view_->SetShowCurtain(false);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      VLOG(1) << "Attempting rollback";
      break;
    case UpdateEngineClient::UPDATE_STATUS_IDLE:
      // Exit update only if update engine was in non-idle status before.
      // Otherwise, it's possible that the update request has not yet been
      // started.
      if (!ignore_idle_status_)
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      break;
    case UpdateEngineClient::UPDATE_STATUS_ERROR:
    case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      // Ignore update errors for non-critical updates to prevent blocking the
      // user from getting to login screen during OOBE if the pending update is
      // not critical.
      if (is_checking_for_update_ || !HasCriticalUpdate()) {
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      } else {
        ExitUpdate(Result::UPDATE_ERROR);
      }
      break;
  }
}

void UpdateScreen::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "UpdateScreen::OnPortalDetectionCompleted(): "
          << "network=" << (network ? network->path() : "") << ", "
          << "state.status=" << state.status << ", "
          << "state.response_code=" << state.response_code;

  // Wait for sane detection results.
  if (network &&
      state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN) {
    return;
  }

  // Restart portal detection for the first notification about offline state.
  if ((!network ||
       state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE) &&
      is_first_detection_notification_) {
    is_first_detection_notification_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          network_portal_detector::GetInstance()->StartPortalDetection(
              false /* force */);
        }));
    return;
  }
  is_first_detection_notification_ = false;

  NetworkPortalDetector::CaptivePortalStatus status = state.status;
  if (state_ == State::STATE_ERROR) {
    // In the case of online state hide error message and proceed to
    // the update stage. Otherwise, update error message content.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)
      StartUpdateCheck();
    else
      UpdateErrorMessage(network, status);
  } else if (state_ == State::STATE_FIRST_PORTAL_CHECK) {
    // In the case of online state immediately proceed to the update
    // stage. Otherwise, prepare and show error message.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      StartUpdateCheck();
    } else {
      UpdateErrorMessage(network, status);

      // StartUpdateCheck, which gets called when the error clears up,  will add
      // the update engine observer back.
      DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);

      if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL)
        DelayErrorMessage();
      else
        ShowErrorMessage();
    }
  }
}

void UpdateScreen::CancelUpdate() {
  VLOG(1) << "Forced update cancel";
  ExitUpdate(Result::UPDATE_NOT_REQUIRED);
}

base::OneShotTimer* UpdateScreen::GetShowTimerForTesting() {
  return &show_timer_;
}

base::OneShotTimer* UpdateScreen::GetErrorMessageTimerForTesting() {
  return &error_message_timer_;
}

base::OneShotTimer* UpdateScreen::GetRebootTimerForTesting() {
  return &reboot_timer_;
}

void UpdateScreen::Show() {
  if (view_) {
#if !defined(OFFICIAL_BUILD)
    view_->SetCancelUpdateShortcutEnabled(true);
#endif
    view_->SetProgress(kBeforeUpdateCheckProgress);
    view_->SetRequiresPermissionForCellular(false);
  }

  show_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kShowDelayMs),
                    base::BindOnce(&UpdateScreen::MakeSureScreenIsShown,
                                   weak_factory_.GetWeakPtr()));

  StartNetworkCheck();
}

void UpdateScreen::Hide() {
  show_timer_.Stop();
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

void UpdateScreen::OnUserAction(const std::string& action_id) {
#if !defined(OFFICIAL_BUILD)
  if (action_id == kUserActionCancelUpdateShortcut)
    CancelUpdate();
  else
#endif
      if (action_id == kUserActionAcceptUpdateOverCellular) {
    DBusThreadManager::Get()
        ->GetUpdateEngineClient()
        ->SetUpdateOverCellularOneTimePermission(
            pending_update_version_, pending_update_size_,
            base::BindRepeating(
                &UpdateScreen::RetryUpdateWithUpdateOverCellularPermissionSet,
                weak_factory_.GetWeakPtr()));
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    // Reset UI context to show curtain again when the user goes back to the
    // update screen.
    if (view_) {
      view_->SetShowCurtain(true);
      view_->SetRequiresPermissionForCellular(false);
    }
    ExitUpdate(Result::UPDATE_ERROR);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void UpdateScreen::RetryUpdateWithUpdateOverCellularPermissionSet(
    bool success) {
  if (success) {
    if (view_)
      view_->SetRequiresPermissionForCellular(false);
    StartUpdateCheck();
  } else {
    // Reset UI context to show curtain again when the user goes back to the
    // update screen.
    if (view_) {
      view_->SetShowCurtain(true);
      view_->SetRequiresPermissionForCellular(false);
    }
    ExitUpdate(Result::UPDATE_ERROR);
  }
}

void UpdateScreen::UpdateDownloadingStats(
    const UpdateEngineClient::Status& status) {
  base::TimeTicks download_current_time = tick_clock_->NowTicks();
  if (download_current_time >=
      download_last_time_ +
          base::TimeDelta::FromSeconds(kMinTimeStepInSeconds)) {
    // Estimate downloading rate.
    double progress_delta =
        std::max(status.download_progress - download_last_progress_, 0.0);
    double time_delta =
        (download_current_time - download_last_time_).InSecondsF();
    double download_rate = status.new_size * progress_delta / time_delta;

    download_last_time_ = download_current_time;
    download_last_progress_ = status.download_progress;

    // Estimate time left.
    double progress_left = std::max(1.0 - status.download_progress, 0.0);
    if (!is_download_average_speed_computed_) {
      download_average_speed_ = download_rate;
      is_download_average_speed_computed_ = true;
    }
    download_average_speed_ =
        kDownloadSpeedSmoothFactor * download_rate +
        (1.0 - kDownloadSpeedSmoothFactor) * download_average_speed_;
    if (download_average_speed_ < kDownloadAverageSpeedDropBound) {
      time_delta = (download_current_time - download_start_time_).InSecondsF();
      download_average_speed_ =
          status.new_size *
          (status.download_progress - download_start_progress_) / time_delta;
    }
    double work_left = progress_left * status.new_size;
    // time_left is in seconds.
    double time_left = work_left / download_average_speed_;
    // |time_left| may be large enough or even +infinity. So we must
    // |bound possible estimations.
    time_left = std::min(time_left, kMaxTimeLeft);

    if (view_) {
      view_->SetShowEstimatedTimeLeft(true);
      view_->SetEstimatedTimeLeft(static_cast<int>(time_left));
    }
  }

  if (view_) {
    int download_progress =
        static_cast<int>(status.download_progress * kDownloadProgressIncrement);
    view_->SetProgress(kBeforeDownloadProgress + download_progress);
  }
}

bool UpdateScreen::HasCriticalUpdate() {
  if (ignore_update_deadlines_)
    return true;

  std::string deadline;
  // Checking for update flag file causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crosbug.com/11106
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath update_deadline_file_path(kUpdateDeadlineFile);
  if (!base::ReadFileToString(update_deadline_file_path, &deadline) ||
      deadline.empty()) {
    return false;
  }

  // TODO(dpolukhin): Analyze file content. Now we can just assume that
  // if the file exists and not empty, there is critical update.
  return true;
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  MakeSureScreenIsShown();
  if (view_)
    view_->SetUpdateCompleted(true);
}

void UpdateScreen::MakeSureScreenIsShown() {
  show_timer_.Stop();

  if (is_shown_ || !view_)
    return;

  is_shown_ = true;
  histogram_helper_->OnScreenShow();

  view_->Show();
}

void UpdateScreen::StartUpdateCheck() {
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  network_portal_detector::GetInstance()->RemoveObserver(this);
  connect_request_subscription_.reset();
  if (state_ == State::STATE_ERROR)
    HideErrorMessage();

  pending_update_version_ = std::string();
  pending_update_size_ = 0;

  state_ = State::STATE_UPDATE;
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
  VLOG(1) << "Initiate update check";
  DBusThreadManager::Get()->GetUpdateEngineClient()->RequestUpdateCheck(
      base::BindRepeating(&UpdateScreen::OnUpdateCheckStarted,
                          weak_factory_.GetWeakPtr()));
}

void UpdateScreen::ShowErrorMessage() {
  LOG(WARNING) << "UpdateScreen::ShowErrorMessage()";

  error_message_timer_.Stop();

  is_shown_ = false;
  show_timer_.Stop();

  state_ = State::STATE_ERROR;
  connect_request_subscription_ =
      error_screen_->RegisterConnectRequestCallback(base::BindRepeating(
          &UpdateScreen::OnConnectRequested, base::Unretained(this)));
  error_screen_->SetUIState(NetworkError::UI_STATE_UPDATE);
  error_screen_->SetParentScreen(UpdateView::kScreenId);
  error_screen_->SetHideCallback(base::BindRepeating(
      &UpdateScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->Show();
  histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
}

void UpdateScreen::HideErrorMessage() {
  LOG(WARNING) << "UpdateScreen::HideErrorMessage()";
  error_screen_->Hide();
  histogram_helper_->OnErrorHide();
}

void UpdateScreen::UpdateErrorMessage(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                   std::string());
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      DCHECK(network);
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                   network->name());
      if (is_first_portal_notification_) {
        is_first_portal_notification_ = false;
        error_screen_->FixCaptivePortal();
      }
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                   std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;

  state_ = State::STATE_ERROR;
  error_message_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kDelayErrorMessageSec), this,
      &UpdateScreen::ShowErrorMessage);
}

void UpdateScreen::OnUpdateCheckStarted(
    UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
  if (result != UpdateEngineClient::UPDATE_RESULT_SUCCESS)
    ExitUpdate(Result::UPDATE_NOT_REQUIRED);
}

void UpdateScreen::OnConnectRequested() {
  if (state_ == State::STATE_ERROR) {
    LOG(WARNING) << "Hiding error message since AP was reselected";
    StartUpdateCheck();
  }
}

void UpdateScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  Show();
}

}  // namespace chromeos
