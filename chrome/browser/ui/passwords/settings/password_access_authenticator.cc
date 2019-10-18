// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_access_authenticator.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

PasswordAccessAuthenticator::PasswordAccessAuthenticator(
    ReauthCallback os_reauth_call)
    : clock_(base::DefaultClock::GetInstance()),
      os_reauth_call_(std::move(os_reauth_call)) {}

PasswordAccessAuthenticator::~PasswordAccessAuthenticator() = default;

// TODO(crbug.com/327331): Trigger Re-Auth after closing and opening the
// settings tab.
void PasswordAccessAuthenticator::EnsureUserIsAuthenticatedAsync(
    password_manager::ReauthPurpose purpose,
    base::OnceCallback<void(bool)> postAuthCallback) {
  const bool can_skip_reauth = CanSkipReauth();
  if (can_skip_reauth) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ReauthToAccessPasswordInSettings",
        password_manager::metrics_util::REAUTH_SKIPPED,
        password_manager::metrics_util::REAUTH_COUNT);
    std::move(postAuthCallback).Run(can_skip_reauth);
    return;
  }

  ForceUserReauthenticationAsync(purpose, std::move(postAuthCallback));
}

void PasswordAccessAuthenticator::ForceUserReauthenticationAsync(
    password_manager::ReauthPurpose purpose,
    base::OnceCallback<void(bool)> postAuthCallback) {
#if defined(OS_WIN)
  // In Windows it is possible to move OS authentication to separate thread.
  ReauthCallback os_reauth_call(os_reauth_call_);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(std::move(os_reauth_call), purpose),
      base::BindOnce(
          &PasswordAccessAuthenticator::ProcessReauthenticationResult,
          weak_ptr_factory_.GetWeakPtr(), purpose,
          std::move(postAuthCallback)));
#else
  // For MAC OS, user input to a process is pulled in by OS prompt,
  // if OS prompt is in open state.
  bool authenticated = os_reauth_call_.Run(purpose);
  ProcessReauthenticationResult(purpose, std::move(postAuthCallback),
                                authenticated);
#endif  // OS_WIN
}

void PasswordAccessAuthenticator::SetOsReauthCallForTesting(
    ReauthCallback os_reauth_call) {
  os_reauth_call_ = std::move(os_reauth_call);
}

void PasswordAccessAuthenticator::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

bool PasswordAccessAuthenticator::CanSkipReauth() const {
  return last_authentication_time_.has_value() &&
         clock_->Now() - *last_authentication_time_ <=
             base::TimeDelta::FromSeconds(kAuthValidityPeriodSeconds);
}

void PasswordAccessAuthenticator::ProcessReauthenticationResult(
    password_manager::ReauthPurpose purpose,
    base::OnceCallback<void(bool)> postAuthCallback,
    bool authenticated) {
  if (authenticated)
    last_authentication_time_ = clock_->Now();
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ReauthToAccessPasswordInSettings",
      authenticated ? password_manager::metrics_util::REAUTH_SUCCESS
                    : password_manager::metrics_util::REAUTH_FAILURE,
      password_manager::metrics_util::REAUTH_COUNT);

  std::move(postAuthCallback).Run(authenticated);
}
