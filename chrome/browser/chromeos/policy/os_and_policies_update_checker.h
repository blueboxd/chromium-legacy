// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_OS_AND_POLICIES_UPDATE_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_OS_AND_POLICIES_UPDATE_CHECKER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/task_executor_with_retries.h"
#include "chromeos/dbus/power/native_timer.h"
#include "chromeos/dbus/update_engine_client.h"

namespace policy {

namespace update_checker_internal {

// The maximum iterations allowed to check for and download an update if the
// operation fails. Used with |os_and_policies_update_checker_|.
constexpr int kMaxOsAndPoliciesUpdateCheckerRetryIterations = 2;

// Interval at which |os_and_policies_update_checker_| retries checking for and
// downloading updates.
constexpr base::TimeDelta kOsAndPoliciesUpdateCheckerRetryTime =
    base::TimeDelta::FromMinutes(30);

}  // namespace update_checker_internal

// This class is used by the scheduled update check policy to perform the actual
// device update check.
class OsAndPoliciesUpdateChecker
    : public chromeos::UpdateEngineClient::Observer {
 public:
  OsAndPoliciesUpdateChecker(
      TaskExecutorWithRetries::GetTicksSinceBootFn get_ticks_since_boot_fn);
  ~OsAndPoliciesUpdateChecker() override;

  using UpdateCheckCompletionCallback = base::OnceCallback<void(bool result)>;

  // Starts an update check and possible download. Once the update check is
  // finished it refreshes policies and finally calls |cb| to indicate success
  // or failure when the process is complete. Overrides any previous calls to
  // |Start|.
  void Start(UpdateCheckCompletionCallback cb);

  // Stops any pending update checks or policy refreshes. Calls
  // |update_check_completion_cb_| with false. It is safe to call |Start| after
  // this.
  void Stop();

 private:
  // Runs |update_check_completion_cb_| with |result| and runs |ResetState|.
  void RunCompletionCallbackAndResetState(bool result);

  // Runs when |update_check_task_executor_::Start| has failed after retries.
  void OnUpdateCheckFailure();

  // Requests update engine to do an update check.
  void StartUpdateCheck();

  // UpdateEngineClient::Observer overrides.
  void UpdateStatusChanged(
      const chromeos::UpdateEngineClient::Status& status) override;

  // Tells whether starting an update check succeeded or not.
  void OnUpdateCheckStarted(
      chromeos::UpdateEngineClient::UpdateCheckResult result);

  // Called when the API call to refresh policies is completed.
  // |update_check_result| represents the result of the update check which
  // triggered this policy refresh.
  void OnRefreshPoliciesCompletion(bool update_check_result);

  // Resets all state and cancels any pending update checks.
  void ResetState();

  // Ignore fist IDLE status that is sent when the update check is initiated.
  bool ignore_idle_status_ = true;

  // Callback passed to |Start|. Called if |StartUpdateCheck| is unsuccessful
  // after retries or when an update check finishes successfully.
  UpdateCheckCompletionCallback update_check_completion_cb_;

  // Scheduled and retries |StartUpdateCheck|.
  TaskExecutorWithRetries update_check_task_executor_;

  base::WeakPtrFactory<OsAndPoliciesUpdateChecker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OsAndPoliciesUpdateChecker);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_OS_AND_POLICIES_UPDATE_CHECKER_H_
