// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"

#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

// static
std::unique_ptr<InSessionPasswordChangeManager>
InSessionPasswordChangeManager::CreateIfEnabled(Profile* primary_profile) {
  if (primary_profile->GetPrefs()->GetBoolean(
          prefs::kSamlInSessionPasswordChangeEnabled)) {
    return std::make_unique<InSessionPasswordChangeManager>(primary_profile);
  }
  return nullptr;
}

InSessionPasswordChangeManager::InSessionPasswordChangeManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)),
      authenticator_(new ChromeCryptohomeAuthenticator(this)) {
  DCHECK(primary_user_);
}

InSessionPasswordChangeManager::~InSessionPasswordChangeManager() {}

void InSessionPasswordChangeManager::StartInSessionPasswordChange() {
  UrgentPasswordExpiryNotificationDialog::Dismiss();
  PasswordChangeDialog::Show(primary_profile_);
}

void InSessionPasswordChangeManager::ChangePassword(
    const std::string& old_password,
    const std::string& new_password) {
  UserContext user_context(*primary_user_);
  user_context.SetKey(Key(new_password));
  authenticator_->MigrateKey(user_context, old_password);
}

void InSessionPasswordChangeManager::HandlePasswordScrapeFailure() {
  // TODO(https://crbug.com/930109): Show different versions of the dialog
  // depending on whether any usable passwords were scraped.
  PasswordChangeDialog::Dismiss();
  ConfirmPasswordChangeDialog::Show();
}

void InSessionPasswordChangeManager::OnAuthSuccess(
    const UserContext& user_context) {
  VLOG(3) << "Cryptohome password is changed.";
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      user_context.GetAccountId(), false);

  // Clear expiration time from prefs so that we don't keep nagging the user to
  // change password (until the SAML provider tells us a new expiration time).
  SamlPasswordAttributes loaded =
      SamlPasswordAttributes::LoadFromPrefs(primary_profile_->GetPrefs());
  SamlPasswordAttributes(
      /*modified_time=*/base::Time::Now(), /*expiration_time=*/base::Time(),
      loaded.password_change_url())
      .SaveToPrefs(primary_profile_->GetPrefs());

  DismissSamlPasswordExpiryNotification(primary_profile_);
  PasswordChangeDialog::Dismiss();
  ConfirmPasswordChangeDialog::Dismiss();
}

void InSessionPasswordChangeManager::OnAuthFailure(const AuthFailure& error) {
  // TODO(https://crbug.com/930109): Ask user for the old password.
  VLOG(1) << "Failed to change cryptohome password: " << error.GetErrorString();
}

}  // namespace chromeos
