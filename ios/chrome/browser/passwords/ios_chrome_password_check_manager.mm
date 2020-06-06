// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using password_manager::CredentialWithPassword;
using password_manager::LeakCheckCredential;
using CompromisedCredentialsView =
    password_manager::CompromisedCredentialsManager::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using State = password_manager::BulkLeakCheckServiceInterface::State;

PasswordCheckState ConvertBulkCheckState(State state) {
  switch (state) {
    case State::kIdle:
      return PasswordCheckState::kIdle;
    case State::kRunning:
      return PasswordCheckState::kRunning;
    case State::kSignedOut:
      return PasswordCheckState::kSignedOut;
    case State::kNetworkError:
      return PasswordCheckState::kOffline;
    case State::kQuotaLimit:
      return PasswordCheckState::kQuotaLimit;
    case State::kCanceled:
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckState::kOther;
  }
  NOTREACHED();
  return PasswordCheckState::kIdle;
}
}  // namespace

IOSChromePasswordCheckManager::IOSChromePasswordCheckManager(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      password_store_(IOSChromePasswordStoreFactory::GetForBrowserState(
          browser_state,
          ServiceAccessType::EXPLICIT_ACCESS)),
      saved_passwords_presenter_(password_store_),
      compromised_credentials_manager_(password_store_,
                                       &saved_passwords_presenter_),
      bulk_leak_check_service_adapter_(
          &saved_passwords_presenter_,
          IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
              browser_state),
          browser_state->GetPrefs()) {
  observed_saved_passwords_presenter_.Add(&saved_passwords_presenter_);
  observed_compromised_credentials_manager_.Add(
      &compromised_credentials_manager_);
  observed_bulk_leak_check_service_.Add(
      IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(browser_state));

  // Instructs the presenter and manager to initialize and build their caches.
  saved_passwords_presenter_.Init();
  compromised_credentials_manager_.Init();
}

IOSChromePasswordCheckManager::~IOSChromePasswordCheckManager() = default;

void IOSChromePasswordCheckManager::StartPasswordCheck() {
  bulk_leak_check_service_adapter_.StartBulkLeakCheck();
  is_check_running_ = true;
}

PasswordCheckState IOSChromePasswordCheckManager::GetPasswordCheckState()
    const {
  if (saved_passwords_presenter_.GetSavedPasswords().empty()) {
    return PasswordCheckState::kNoPasswords;
  }
  return ConvertBulkCheckState(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

base::Time IOSChromePasswordCheckManager::GetLastPasswordCheckTime() const {
  return base::Time::FromDoubleT(browser_state_->GetPrefs()->GetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted));
}

std::vector<CredentialWithPassword>
IOSChromePasswordCheckManager::GetCompromisedCredentials() const {
  return compromised_credentials_manager_.GetCompromisedCredentials();
}

void IOSChromePasswordCheckManager::OnSavedPasswordsChanged(
    SavedPasswordsView) {
  // Observing saved passwords to update possible kNoPasswords state.
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::OnCompromisedCredentialsChanged(
    CompromisedCredentialsView credentials) {
  for (auto& observer : observers_) {
    observer.CompromisedCredentialsChanged(credentials);
  }
}

void IOSChromePasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Saving time of last successful password check
    browser_state_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());
  }
  if (state != State::kRunning) {
    is_check_running_ = false;
  }
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    compromised_credentials_manager_.SaveCompromisedCredential(credential);
  }
}

void IOSChromePasswordCheckManager::NotifyPasswordCheckStatusChanged() {
  for (auto& observer : observers_) {
    observer.PasswordCheckStatusChanged(GetPasswordCheckState());
  }
}
