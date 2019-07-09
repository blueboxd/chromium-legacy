// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/os_and_policies_update_checker.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/task_executor_with_retries.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

namespace {

// The tag associated to register |update_check_task_executor_|.
constexpr char kUpdateCheckTaskExecutorTag[] = "UpdateCheckTaskExecutor";

}  // namespace

OsAndPoliciesUpdateChecker::OsAndPoliciesUpdateChecker(
    TaskExecutorWithRetries::GetTicksSinceBootFn get_ticks_since_boot_fn)
    : update_check_task_executor_(
          kUpdateCheckTaskExecutorTag,
          std::move(get_ticks_since_boot_fn),
          update_checker_internal::
              kMaxOsAndPoliciesUpdateCheckerRetryIterations,
          update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime) {}

OsAndPoliciesUpdateChecker::~OsAndPoliciesUpdateChecker() = default;

void OsAndPoliciesUpdateChecker::Start(UpdateCheckCompletionCallback cb) {
  // Override any previous calls by resetting state.
  ResetState();
  // Must be set before starting the task runner, as callbacks may be called
  // synchronously.
  update_check_completion_cb_ = std::move(cb);
  // Safe to use "this" as |update_check_task_executor_| is a member of this
  // class.
  update_check_task_executor_.Start(
      base::BindRepeating(&OsAndPoliciesUpdateChecker::StartUpdateCheck,
                          base::Unretained(this)),
      base::BindOnce(&OsAndPoliciesUpdateChecker::OnUpdateCheckFailure,
                     base::Unretained(this)));
}

void OsAndPoliciesUpdateChecker::Stop() {
  ResetState();
}

void OsAndPoliciesUpdateChecker::OnUpdateCheckFailure() {
  // Refresh policies after the update check is finished successfully or
  // unsuccessfully.
  g_browser_process->policy_service()->RefreshPolicies(base::BindRepeating(
      &OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion,
      weak_factory_.GetWeakPtr(), false /* update_check_result */));
}

void OsAndPoliciesUpdateChecker::RunCompletionCallbackAndResetState(
    bool result) {
  if (update_check_completion_cb_)
    std::move(update_check_completion_cb_).Run(result);
  ResetState();
}

void OsAndPoliciesUpdateChecker::StartUpdateCheck() {
  // Only one update check can be pending at any time.
  weak_factory_.InvalidateWeakPtrs();

  // This could be a retry, reset observer state as adding observers is not
  // idempotent.
  chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(
      this);

  // Register observer to keep track of different stages of the update check.
  chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(
      this);
  chromeos::DBusThreadManager::Get()
      ->GetUpdateEngineClient()
      ->RequestUpdateCheck(
          base::BindRepeating(&OsAndPoliciesUpdateChecker::OnUpdateCheckStarted,
                              weak_factory_.GetWeakPtr()));

  // Do nothing for the initial idle stage when the update check state machine
  // has just started.
  ignore_idle_status_ = true;
}

void OsAndPoliciesUpdateChecker::UpdateStatusChanged(
    const chromeos::UpdateEngineClient::Status& status) {
  // Only ignore idle state if it is the first and only non-error state in the
  // state machine.
  if (ignore_idle_status_ &&
      status.status > chromeos::UpdateEngineClient::UPDATE_STATUS_IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.status) {
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_IDLE:
      // Exit update only if update engine was in non-idle status before i.e.
      // no update to download. Otherwise, it's possible that the update request
      // has not yet been started.
      if (!ignore_idle_status_)
        RunCompletionCallbackAndResetState(true);
      break;

    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_UPDATED_NEED_REBOOT:
      // Refresh policies after the update check is finished successfully or
      // unsuccessfully.
      g_browser_process->policy_service()->RefreshPolicies(base::BindRepeating(
          &OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion,
          weak_factory_.GetWeakPtr(), true /* update_check_result */));
      break;

    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_ERROR:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_REPORTING_ERROR_EVENT:
      update_check_task_executor_.ScheduleRetry();
      break;

    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_FINALIZING:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_VERIFYING:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_DOWNLOADING:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_UPDATE_AVAILABLE:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_CHECKING_FOR_UPDATE:
    case chromeos::UpdateEngineClient::UpdateStatusOperation::
        UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      // Do nothing on intermediate states.
      break;
  }
}

void OsAndPoliciesUpdateChecker::OnUpdateCheckStarted(
    chromeos::UpdateEngineClient::UpdateCheckResult result) {
  switch (result) {
    case chromeos::UpdateEngineClient::UPDATE_RESULT_SUCCESS:
      // Nothing to do if the update check started successfully.
      break;
    case chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED:
      update_check_task_executor_.ScheduleRetry();
      break;
    case chromeos::UpdateEngineClient::UPDATE_RESULT_NOTIMPLEMENTED:
      // No point retrying if the operation is not implemented. Refresh policies
      // since the update check is done.
      LOG(ERROR) << "Update check failed: Operation not implemented";
      g_browser_process->policy_service()->RefreshPolicies(base::BindRepeating(
          &OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion,
          weak_factory_.GetWeakPtr(), false /* update_check_result */));
      break;
  }
}

void OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion(
    bool update_check_result) {
  RunCompletionCallbackAndResetState(update_check_result);
}

void OsAndPoliciesUpdateChecker::ResetState() {
  weak_factory_.InvalidateWeakPtrs();
  chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(
      this);
  update_check_task_executor_.Stop();
  ignore_idle_status_ = true;
}

}  // namespace policy
