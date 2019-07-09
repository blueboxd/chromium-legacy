// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include <memory>

#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"

namespace password_manager {

StubPasswordManagerClient::StubPasswordManagerClient()
    : ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()) {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  return false;
}

void StubPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool update_password) {}

void StubPasswordManagerClient::HideManualFallbackForSaving() {}

void StubPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::mojom::FocusedFieldType focused_field_type) {}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {}

void StubPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {}

void StubPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {}

void StubPasswordManagerClient::NotifyStorePasswordCalled() {}

void StubPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_manager) {}

PrefService* StubPasswordManagerClient::GetPrefs() const {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetPasswordStore() const {
  return nullptr;
}

const GURL& StubPasswordManagerClient::GetLastCommittedEntryURL() const {
  return GURL::EmptyGURL();
}

const CredentialsFilter* StubPasswordManagerClient::GetStoreResultFilter()
    const {
  return &credentials_filter_;
}

const LogManager* StubPasswordManagerClient::GetLogManager() const {
  return &log_manager_;
}

#if defined(FULL_SAFE_BROWSING)
safe_browsing::PasswordProtectionService*
StubPasswordManagerClient::GetPasswordProtectionService() const {
  return nullptr;
}

void StubPasswordManagerClient::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {}

void StubPasswordManagerClient::CheckProtectedPasswordEntry(
    metrics_util::PasswordType reused_password_type,
    const std::string& username,
    const std::vector<std::string>& matching_domains,
    bool password_field_exists) {}

void StubPasswordManagerClient::LogPasswordReuseDetectedEvent() {}
#endif

ukm::SourceId StubPasswordManagerClient::GetUkmSourceId() {
  return ukm_source_id_;
}

PasswordManagerMetricsRecorder*
StubPasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId(), GetMainFrameURL());
  }
  return base::OptionalOrNullptr(metrics_recorder_);
}

bool StubPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool StubPasswordManagerClient::IsNewTabPage() const {
  return false;
}

}  // namespace password_manager
