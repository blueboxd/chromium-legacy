// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace base {
class TickClock;
}

namespace chromeos {

class ErrorScreen;
class ErrorScreensHistogramHelper;
class NetworkState;
class ScreenManager;
class UpdateView;

// Controller for the update screen.
//
// The screen will request an update availability check from the update engine,
// and track the update engine progress. When the UpdateScreen finishes, it will
// run the |exit_callback| with the screen result.
//
// If the update engine reports no updates are found, or the available
// update is not critical, UpdateScreen will report UPDATE_NOT_REQUIRED result.
//
// If the update engine reports an error while performing a critical update,
// UpdateScreen will report UPDATE_ERROR result.
//
// If the update engine reports that update is blocked because it would be
// performed over a metered network, UpdateScreen will request user consent
// before proceeding with the update. If the user rejects, UpdateScreen will
// exit and report UPDATE_ERROR result.
//
// If update engine finds a critical update, UpdateScreen will wait for the
// update engine to apply the update, and then request a reboot (if reboot
// request times out, a message requesting manual reboot will be shown to the
// user).
//
// Before update check request is made, the screen will ensure that the device
// has network connectivity - if the current network is not online (e.g. behind
// a protal), it will request an ErrorScreen to be shown. Update check will be
// delayed until the Internet connectivity is established.
class UpdateScreen : public BaseScreen,
                     public UpdateEngineClient::Observer,
                     public NetworkPortalDetector::Observer {
 public:
  static UpdateScreen* Get(ScreenManager* manager);

  enum class Result {
    UPDATE_NOT_REQUIRED,
    UPDATE_ERROR,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  UpdateScreen(UpdateView* view,
               ErrorScreen* error_screen,
               const ScreenExitCallback& exit_callback);
  ~UpdateScreen() override;

  // Called when the being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before it.
  void OnViewDestroyed(UpdateView* view);

  void SetIgnoreIdleStatus(bool ignore_idle_status);

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const UpdateEngineClient::Status& status) override;

  // NetworkPortalDetector::Observer implementation:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // Skip update UI, usually used only in debug builds/tests.
  void CancelUpdate();

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  base::OneShotTimer* GetShowTimerForTesting();
  base::OneShotTimer* GetErrorMessageTimerForTesting();
  base::OneShotTimer* GetRebootTimerForTesting();

  void set_ignore_update_deadlines_for_testing(bool ignore_update_deadlines) {
    ignore_update_deadlines_ = ignore_update_deadlines;
  }

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 protected:
  // Reports update results.
  void ExitUpdate(Result result);

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestAPReselection);
  friend class UpdateScreenUnitTest;

  enum class State {
    STATE_IDLE = 0,
    STATE_FIRST_PORTAL_CHECK,
    STATE_REQUESTING_USER_PERMISSION,
    STATE_UPDATE,
    STATE_ERROR
  };

  // Starts network check.
  void StartNetworkCheck();

  // Callback to UpdateEngineClient::SetUpdateOverCellularOneTimePermission
  // called in response to user confirming that the OS update can proceed
  // despite being over cellular charges.
  // |success|: whether the update engine accepted the user permission.
  void RetryUpdateWithUpdateOverCellularPermissionSet(bool success);

  // Updates downloading stats (remaining time and downloading
  // progress) on the AU screen.
  void UpdateDownloadingStats(const UpdateEngineClient::Status& status);

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Timer notification handlers.
  void OnWaitForRebootTimeElapsed();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  void StartUpdateCheck();
  void ShowErrorMessage();
  void HideErrorMessage();
  void UpdateErrorMessage(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status);

  void DelayErrorMessage();

  // Callback for UpdateEngineClient::RequestUpdateCheck() called fomr
  // StartUpdateCheck().
  void OnUpdateCheckStarted(UpdateEngineClient::UpdateCheckResult result);

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  // Callback passed to |error_screen_| when it's shown. Called when the error
  // screen gets hidden.
  void OnErrorScreenHidden();

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer reboot_timer_;

  // Current state of the update screen.
  State state_ = State::STATE_IDLE;

  const base::TickClock* tick_clock_;

  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  int reboot_check_delay_ = 0;

  // True if in the process of checking for update.
  bool is_checking_for_update_ = true;
  // Flag that is used to detect when update download has just started.
  bool is_downloading_update_ = false;
  // If true, update deadlines are ignored.
  // Note, this is false by default.
  bool ignore_update_deadlines_ = false;
  // Whether the update screen is shown.
  bool is_shown_ = false;
  // Ignore fist IDLE status that is sent before update screen initiated check.
  bool ignore_idle_status_ = true;

  UpdateView* view_;
  ErrorScreen* error_screen_;
  ScreenExitCallback exit_callback_;

  // Time of the first notification from the downloading stage.
  base::TimeTicks download_start_time_;
  double download_start_progress_ = 0;

  // Time of the last notification from the downloading stage.
  base::TimeTicks download_last_time_;
  double download_last_progress_ = 0;

  bool is_download_average_speed_computed_ = false;
  double download_average_speed_ = 0;

  // True if there was no notification from NetworkPortalDetector
  // about state for the default network.
  bool is_first_detection_notification_ = true;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  // Information about a pending update. Set if a user permission is required to
  // proceed with the update. The values have to be passed to the update engine
  // in SetUpdateOverCellularOneTimePermission method in order to enable update
  // over cellular network.
  std::string pending_update_version_;
  int64_t pending_update_size_ = 0;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  // Showing the update screen view will be delayed for a small amount of time
  // after UpdateScreen::Show() is called. If the screen determines that an
  // update is not required before the delay expires, the UpdateScreen will exit
  // without actually showing any UI. The goal is to avoid short flashes of
  // update screen UI when update check is done quickly enough.
  // This holds the timer to show the actual update screen UI.
  base::OneShotTimer show_timer_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  ErrorScreen::ConnectRequestCallbackSubscription connect_request_subscription_;

  base::WeakPtrFactory<UpdateScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
