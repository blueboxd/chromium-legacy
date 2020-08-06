// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include "chrome/browser/password_check/android/password_check_bridge.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/sync/driver/profile_sync_service.h"

using PasswordCheckUIStatus = password_manager::PasswordCheckUIStatus;
using State = password_manager::BulkLeakCheckService::State;
using SyncState = password_manager::SyncState;

PasswordCheckManager::PasswordCheckManager(Profile* profile, Observer* observer)
    : observer_(observer), profile_(profile) {
  observed_saved_passwords_presenter_.Add(&saved_passwords_presenter_);
  observed_compromised_credentials_manager_.Add(
      &compromised_credentials_manager_);
  observed_bulk_leak_check_service_.Add(
      BulkLeakCheckServiceFactory::GetForProfile(profile));

  // Instructs the presenter and provider to initialize and built their caches.
  // This will soon after invoke OnCompromisedCredentialsChanged(). Calls to
  // GetCompromisedCredentials() that might happen until then will return an
  // empty list.
  saved_passwords_presenter_.Init();
  compromised_credentials_manager_.Init();
}

PasswordCheckManager::~PasswordCheckManager() = default;

void PasswordCheckManager::StartCheck() {
  if (!is_initialized_) {
    was_start_requested_ = true;
    return;
  }

  // The request is being handled, so reset the boolean.
  was_start_requested_ = false;
  bulk_leak_check_service_adapter_.StartBulkLeakCheck();
}

void PasswordCheckManager::StopCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
}

int PasswordCheckManager::GetCompromisedCredentialsCount() const {
  return compromised_credentials_manager_.GetCompromisedCredentials().size();
}

int PasswordCheckManager::GetSavedPasswordsCount() const {
  return saved_passwords_presenter_.GetSavedPasswords().size();
}

void PasswordCheckManager::OnSavedPasswordsChanged(
    password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords) {
  if (!is_initialized_) {
    observer_->OnSavedPasswordsFetched(passwords.size());
    is_initialized_ = true;
  }

  if (passwords.empty()) {
    observer_->OnPasswordCheckStatusChanged(
        PasswordCheckUIStatus::kErrorNoPasswords);
    was_start_requested_ = false;
    return;
  }

  if (was_start_requested_) {
    StartCheck();
  }
}

void PasswordCheckManager::OnCompromisedCredentialsChanged(
    password_manager::CompromisedCredentialsManager::CredentialsView
        credentials) {
  observer_->OnCompromisedCredentialsChanged(credentials.size());
}

void PasswordCheckManager::OnStateChanged(State state) {
  observer_->OnPasswordCheckStatusChanged(GetUIStatus(state));
}

void PasswordCheckManager::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  // TODO(crbug.com/1102025): implement this.
}

PasswordCheckUIStatus PasswordCheckManager::GetUIStatus(State state) const {
  switch (state) {
    case State::kIdle:
      return PasswordCheckUIStatus::kIdle;
    case State::kRunning:
      return PasswordCheckUIStatus::kRunning;
    case State::kSignedOut:
      return PasswordCheckUIStatus::kErrorSignedOut;
    case State::kNetworkError:
      return PasswordCheckUIStatus::kErrorOffline;
    case State::kQuotaLimit:
      return CanUseAccountCheck()
                 ? PasswordCheckUIStatus::kErrorQuotaLimitAccountCheck
                 : PasswordCheckUIStatus::kErrorQuotaLimit;
    case State::kCanceled:
      return PasswordCheckUIStatus::kCanceled;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckUIStatus::kErrorUnknown;
  }
  NOTREACHED();
  return PasswordCheckUIStatus::kIdle;
}

bool PasswordCheckManager::CanUseAccountCheck() const {
  SyncState sync_state = password_manager_util::GetPasswordSyncState(
      ProfileSyncServiceFactory::GetForProfile(profile_));
  return sync_state == SyncState::SYNCING_NORMAL_ENCRYPTION ||
         sync_state == SyncState::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION;
}
