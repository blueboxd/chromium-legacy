// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/util/timer/wall_clock_timer.h"
#include "base/version.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chromeos/dbus/update_engine_client.h"

class PrefRegistrySimple;

namespace base {
class Clock;
class DictionaryValue;
class Time;
}

namespace policy {

// This class observes the device setting |kMinimumChromeVersionEnforced|, and
// checks if respective requirement is met.
class MinimumVersionPolicyHandler : public BuildStateObserver {
 public:
  static const char kChromeVersion[];
  static const char kWarningPeriod[];
  static const char KEolWarningPeriod[];

  class Observer {
   public:
    virtual void OnMinimumVersionStateChanged() = 0;
    virtual ~Observer() = default;
  };

  // Delegate of MinimumVersionPolicyHandler to handle the external
  // dependencies.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Checks if the user is logged in as any kiosk app.
    virtual bool IsKioskMode() const = 0;

    // Checks if the device is enterprise managed.
    virtual bool IsEnterpriseManaged() const = 0;

    // Checks if a user is logged in.
    virtual bool IsUserLoggedIn() const = 0;

    // Checks if the user logged in is a managed user.
    virtual bool IsUserManaged() const = 0;

    // Checks if we are currently on the login screen.
    virtual bool IsLoginSessionState() const = 0;

    // Checks if login is in progress before starting user session.
    virtual bool IsLoginInProgress() const = 0;

    // Shows the update required screen.
    virtual void ShowUpdateRequiredScreen() = 0;

    // Terminates the current session and restarts Chrome to show the login
    // screen.
    virtual void RestartToLoginScreen() = 0;

    // Hides update required screen and shows the login screen.
    virtual void HideUpdateRequiredScreenIfShown() = 0;

    virtual const base::Version& GetCurrentVersion() const = 0;
  };

  class MinimumVersionRequirement {
   public:
    MinimumVersionRequirement(const base::Version version,
                              const base::TimeDelta warning,
                              const base::TimeDelta eol_warning);

    MinimumVersionRequirement(const MinimumVersionRequirement&) = delete;

    // Method used to create an instance of MinimumVersionRequirement from
    // dictionary if it contains valid version string.
    static std::unique_ptr<MinimumVersionRequirement> CreateInstanceIfValid(
        const base::DictionaryValue* dict);

    // This is used to compare two MinimumVersionRequirement objects
    // and returns 1 if the first object has version or warning time
    // or eol warning time greater than that the second, -1 if the
    // its version or warning time or eol warning time is less than the second,
    // else 0.
    int Compare(const MinimumVersionRequirement* other) const;

    base::Version version() const { return minimum_version_; }
    base::TimeDelta warning() const { return warning_time_; }
    base::TimeDelta eol_warning() const { return eol_warning_time_; }

   private:
    base::Version minimum_version_;
    base::TimeDelta warning_time_;
    base::TimeDelta eol_warning_time_;
  };

  explicit MinimumVersionPolicyHandler(Delegate* delegate,
                                       chromeos::CrosSettings* cros_settings);
  ~MinimumVersionPolicyHandler() override;

  // BuildStateObserver
  void OnUpdate(const BuildState* build_state) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool RequirementsAreSatisfied() const { return requirements_met_; }

  // Returns |true| if the current version satisfies the given requirement.
  bool CurrentVersionSatisfies(
      const MinimumVersionRequirement& requirement) const;

  const MinimumVersionRequirement* GetState() const { return state_.get(); }

  bool DeadlineReached() { return deadline_reached; }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Callback used in tests and invoked after end-of-life status has been
  // fetched from the update_engine.
  void set_fetch_eol_callback_for_testing(base::OnceClosure callback) {
    fetch_eol_callback_ = std::move(callback);
  }

  bool IsDeadlineTimerRunningForTesting();

 private:
  void OnPolicyChanged();
  bool IsPolicyApplicable();
  void Reset();

  // Handles the state when update is required as per the policy. If on the
  // login screen, update required screen is shown, else the user is logged out
  // if the device is not updated within the given |warning_time|. The
  // |warning_time| is supplied by the policy.
  void HandleUpdateRequired(base::TimeDelta warning_time);

  void HandleUpdateNotRequired();

  // Invokes update_engine_client to fetch the end-of-life status of the device.
  void FetchEolInfo();

  // Callback after fetching end-of-life info from the update_engine_client.
  void OnFetchEolInfo(chromeos::UpdateEngineClient::EolInfo info);

  // Called when the warning time to apply updates has expired. If the user on
  // the login screen, the update required screen is shown else the current user
  // session is terminated to bring the user back to the login screen.
  void OnDeadlineReached();

  // Starts the timer to expire when |deadline| is reached.
  void StartDeadlineTimer(base::Time deadline);

  // Starts observing the BuildState for any updates in Chrome and resets the
  // state if new version satisfies the minimum version requirement.
  void StartObservingUpdate();

  // Updates pref |kUpdateRequiredWarningPeriod| in local state to
  // |warning_time|. If |kUpdateRequiredTimerStartTime| is not null, it means
  // update is already required and hence, the timer start time should not be
  // updated.
  void UpdateLocalState(base::TimeDelta warning_time);

  // Resets the local state prefs to default values.
  void ResetLocalState();

  // This delegate instance is owned by the owner of
  // MinimumVersionPolicyHandler. The owner is responsible to make sure that the
  // delegate lives throughout the life of the policy handler.
  Delegate* delegate_;

  // This represents the current minimum version requirement.
  // It is chosen as one of the configurations specified in the policy. It is
  // set to nullptr if the current version is higher than the minimum required
  // version in all the configurations.
  std::unique_ptr<MinimumVersionRequirement> state_;

  bool requirements_met_ = true;
  bool eol_reached_ = false;

  // If this flag is true, user should restricted to use the session by logging
  // out and/or showing update required screen.
  bool deadline_reached = false;

  base::Time update_required_time_;

  // Fires when the deadline to update the device has reached or passed.
  util::WallClockTimer update_required_deadline_timer_;

  // Non-owning reference to CrosSettings. This class have shorter lifetime than
  // CrosSettings.
  chromeos::CrosSettings* cros_settings_;

  base::Clock* const clock_;

  base::OnceClosure fetch_eol_callback_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      policy_subscription_;

  // List of registered observers.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<MinimumVersionPolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MinimumVersionPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_HANDLER_H_
