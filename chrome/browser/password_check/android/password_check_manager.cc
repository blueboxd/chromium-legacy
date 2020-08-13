// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_check/android/password_check_bridge.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_manager.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kWellKnownUrlPath[] = ".well-known/change-password";

base::string16 GetDisplayUsername(const base::string16& username) {
  return username.empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : username;
}

std::string CreateChangeUrl(const GURL& url) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    return url.GetOrigin().spec() + kWellKnownUrlPath;
  }
  return url.GetOrigin().spec();
}

}  // namespace

using autofill::PasswordForm;

using CredentialsView =
    password_manager::CompromisedCredentialsManager::CredentialsView;
using PasswordCheckUIStatus = password_manager::PasswordCheckUIStatus;
using State = password_manager::BulkLeakCheckService::State;
using SyncState = password_manager::SyncState;
using CredentialWithPassword = password_manager::CredentialWithPassword;
using CompromisedCredentialForUI =
    PasswordCheckManager::CompromisedCredentialForUI;

CompromisedCredentialForUI::CompromisedCredentialForUI(
    const CredentialWithPassword& credential)
    : CredentialWithPassword(credential) {}

CompromisedCredentialForUI::CompromisedCredentialForUI(
    const CompromisedCredentialForUI& other) = default;
CompromisedCredentialForUI::CompromisedCredentialForUI(
    CompromisedCredentialForUI&& other) = default;
CompromisedCredentialForUI& CompromisedCredentialForUI::operator=(
    const CompromisedCredentialForUI& other) = default;
CompromisedCredentialForUI& CompromisedCredentialForUI::operator=(
    CompromisedCredentialForUI&& other) = default;
CompromisedCredentialForUI::~CompromisedCredentialForUI() = default;

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

std::vector<CompromisedCredentialForUI>
PasswordCheckManager::GetCompromisedCredentials() const {
  std::vector<CredentialWithPassword> credentials =
      compromised_credentials_manager_.GetCompromisedCredentials();
  std::vector<CompromisedCredentialForUI> ui_credentials;
  ui_credentials.reserve(credentials.size());
  for (const auto& credential : credentials) {
    ui_credentials.push_back(MakeUICredential(credential));
  }
  return ui_credentials;
}

void PasswordCheckManager::RemoveCredential(
    const password_manager::CredentialView& credential) {
  compromised_credentials_manager_.RemoveCompromisedCredential(credential);
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
  // TODO(crbug.com/1092444): Advance progress.
  if (is_leaked) {
    // TODO(crbug.com/1092444): Trigger single-credential update.
    compromised_credentials_manager_.SaveCompromisedCredential(credential);
  }
}

CompromisedCredentialForUI PasswordCheckManager::MakeUICredential(
    const CredentialWithPassword& credential) const {
  CompromisedCredentialForUI ui_credential(credential);
  auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
      credential.signon_realm);

  ui_credential.display_username = GetDisplayUsername(credential.username);
  if (facet.IsValidAndroidFacetURI()) {
    const PasswordForm& android_form =
        compromised_credentials_manager_.GetSavedPasswordsFor(credential)[0];

    ui_credential.is_android_credential = true;
    ui_credential.package_name = facet.android_package_name();

    if (android_form.app_display_name.empty()) {
      // In case no affiliation information could be obtained show the
      // formatted package name to the user.
      ui_credential.display_origin = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
    } else {
      ui_credential.display_origin =
          base::UTF8ToUTF16(android_form.app_display_name);
    }
  } else {
    ui_credential.is_android_credential = false;
    ui_credential.display_origin = url_formatter::FormatUrl(
        credential.url.GetOrigin(),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
    ui_credential.change_password_url = CreateChangeUrl(ui_credential.url);
  }

  return ui_credential;
}

void PasswordCheckManager::OnBulkCheckServiceShutDown() {
  observed_bulk_leak_check_service_.Remove(
      BulkLeakCheckServiceFactory::GetForProfile(profile_));
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
