// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

using metrics_util::LogPasswordSettingsReauthResult;
using metrics_util::ReauthResult;

// static
constexpr base::TimeDelta PasswordAccessAuthenticator::kAuthValidityPeriod;

PasswordAccessAuthenticator::PasswordAccessAuthenticator(
    ReauthCallback os_reauth_call)
    : os_reauth_call_(std::move(os_reauth_call)) {}

PasswordAccessAuthenticator::~PasswordAccessAuthenticator() = default;

// TODO(crbug.com/327331): Trigger Re-Auth after closing and opening the
// settings tab.
bool PasswordAccessAuthenticator::EnsureUserIsAuthenticated(
    ReauthPurpose purpose) {
  if (base::Time::Now() <= last_authentication_time_ + kAuthValidityPeriod) {
    LogPasswordSettingsReauthResult(ReauthResult::kSkipped);
    return true;
  }

  return ForceUserReauthentication(purpose);
}

bool PasswordAccessAuthenticator::ForceUserReauthentication(
    ReauthPurpose purpose) {
  const bool authenticated = os_reauth_call_.Run(purpose);
  if (authenticated)
    last_authentication_time_ = base::Time::Now();

  LogPasswordSettingsReauthResult(authenticated ? ReauthResult::kSuccess
                                                : ReauthResult::kFailure);
  return authenticated;
}

}  // namespace password_manager
