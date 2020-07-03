// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/in_session_password_sync_manager.h"

#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_base.h"

namespace chromeos {

InSessionPasswordSyncManager::InSessionPasswordSyncManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      clock_(base::DefaultClock::GetInstance()),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)) {
  DCHECK(primary_user_);

  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may be not initialized in some unit
  // tests
  if (session_manager) {
    session_manager->AddObserver(this);
  }

  screenlock_bridge_ = proximity_auth::ScreenlockBridge::Get();
  DCHECK(screenlock_bridge_);
}

InSessionPasswordSyncManager::~InSessionPasswordSyncManager() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

bool InSessionPasswordSyncManager::IsLockReauthEnabled() {
  PrefService* prefs = primary_profile_->GetPrefs();
  return prefs->GetBoolean(prefs::kSamlLockScreenReauthenticationEnabled);
}

void InSessionPasswordSyncManager::MaybeForceReauthOnLockScreen() {
  if (enforce_reauth_on_lock_) {
    // Re-authentication already enforced, no other action is needed.
    return;
  }
  if (!primary_user_->force_online_signin()) {
    // force_online_signin flag is not set - do not update the screen.
    return;
  }
  if (screenlock_bridge_->IsLocked()) {
    // On the lock screen: need to update the UI.
    screenlock_bridge_->lock_handler()->SetAuthType(
        primary_user_->GetAccountId(),
        proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, base::string16());
  }
  enforce_reauth_on_lock_ = true;
}

void InSessionPasswordSyncManager::OnAuthSucceeded(
    const UserContext& user_context) {
  if (user_context.GetAccountId() != primary_user_->GetAccountId()) {
    // Tried to re-authenicate with non-primary user: the authentication was
    // successful but we are allowed to unlock only with valid credentials of
    // the user who locked the screen. In this case show customized version
    // of first re-auth flow dialog with an error message.
    // TODO(crbug.com/1090341)
    return;
  }

  UpdateOnlineAuth();
  enforce_reauth_on_lock_ = false;
  if (screenlock_bridge_->IsLocked()) {
    screenlock_bridge_->lock_handler()->Unlock(user_context.GetAccountId());
  }
}

void InSessionPasswordSyncManager::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

void InSessionPasswordSyncManager::Shutdown() {}

void InSessionPasswordSyncManager::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    // We are unlocking the session, no further action required.
    return;
  }
  if (!enforce_reauth_on_lock_) {
    // locking the session but no re-auth flag set - show standard UI.
    return;
  }

  // Request re-auth immediately after locking the screen.
  screenlock_bridge_->lock_handler()->SetAuthType(
      primary_user_->GetAccountId(),
      proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, base::string16());
}

void InSessionPasswordSyncManager::UpdateOnlineAuth() {
  PrefService* prefs = primary_profile_->GetPrefs();
  const base::Time now = clock_->Now();
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, now);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), false);
  user_manager::known_user::SetLastOnlineSignin(primary_user_->GetAccountId(),
                                                now);
}

}  // namespace chromeos
