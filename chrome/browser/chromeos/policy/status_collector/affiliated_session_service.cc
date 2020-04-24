// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/affiliated_session_service.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace policy {

namespace {
AffiliatedSessionService* g_instance = nullptr;

constexpr base::TimeDelta kMinimumSuspendDuration =
    base::TimeDelta::FromMinutes(1);

bool IsPrimaryAndAffiliated(Profile* profile) {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  bool is_primary = chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile);
  bool is_affiliated = user && user->IsAffiliated();
  if (!is_primary || !is_affiliated) {
    VLOG(1) << "The profile for the primary user is not associated with an "
               "affiliated user.";
  }
  return is_primary && is_affiliated;
}

}  // namespace

AffiliatedSessionService::AffiliatedSessionService(base::Clock* clock)
    : clock_(clock), session_manager_(session_manager::SessionManager::Get()) {
  DCHECK(!g_instance);
  session_manager_->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  is_session_locked_ = session_manager_->IsScreenLocked();
  g_instance = this;
}

AffiliatedSessionService::~AffiliatedSessionService() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AffiliatedSessionService* AffiliatedSessionService::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void AffiliatedSessionService::AddObserver(
    AffiliatedSessionService::Observer* observer) {
  observers_.AddObserver(observer);
}

void AffiliatedSessionService::RemoveObserver(
    AffiliatedSessionService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AffiliatedSessionService::OnSessionStateChanged() {
  bool is_session_locked = session_manager_->IsScreenLocked();
  if (is_session_locked_ == is_session_locked) {
    return;
  }
  is_session_locked_ = is_session_locked;

  if (is_session_locked_) {
    for (auto& observer : observers_) {
      observer.OnLocked();
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnUnlocked();
    }
  }
}

void AffiliatedSessionService::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!IsPrimaryAndAffiliated(profile)) {
    return;
  }
  profile->AddObserver(this);
  for (auto& observer : observers_) {
    observer.OnAffiliatedLogin(profile);
  }
}

void AffiliatedSessionService::OnProfileWillBeDestroyed(Profile* profile) {
  is_session_locked_ = false;
  if (!IsPrimaryAndAffiliated(profile)) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnAffiliatedLogout(profile);
  }
  profile->RemoveObserver(this);
}

void AffiliatedSessionService::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  if (sleep_duration < kMinimumSuspendDuration) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnResumeActive(clock_->Now() - sleep_duration);
  }
}

}  // namespace policy
