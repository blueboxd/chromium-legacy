// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"

namespace metrics {

class MetricProvider;
class SampledProfile;

// Provides access to ChromeOS profile data using different metric collectors.
// It detects certain system triggers, such as device resuming from suspend
// mode, or user logging in, which it forwards to the registered collectors.
class ProfileProvider : public chromeos::PowerManagerClient::Observer,
                        public chromeos::LoginState::Observer {
 public:
  ProfileProvider();
  ~ProfileProvider() override;

  void Init();

  // Stores collected perf data protobufs in |sampled_profiles|. Clears all the
  // stored profile data. Returns true if it wrote to |sampled_profiles|.
  bool GetSampledProfiles(std::vector<SampledProfile>* sampled_profiles);

 protected:
  // Called when either the login state or the logged in user type changes.
  // Activates the registered collectors to start collecting. Inherited from
  // LoginState::Observer.
  void LoggedInStateChanged() override;

  // Called when a suspend finishes. This is either a successful suspend
  // followed by a resume, or a suspend that was canceled. Inherited from
  // PowerManagerClient::Observer.
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  // Vector of registered metric collectors.
  std::vector<std::unique_ptr<MetricProvider>> collectors_;

 private:
  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription object's destructor will automatically unregister the
  // callback in SessionRestore, so that the callback list does not contain any
  // obsolete callbacks.
  SessionRestore::CallbackSubscription
      on_session_restored_callback_subscription_;

  // To pass around the "this" pointer across threads safely.
  base::WeakPtrFactory<ProfileProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileProvider);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_
