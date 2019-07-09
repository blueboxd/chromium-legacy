// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"

#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"
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
  } else {
    // If the policy is disabled, clear the SAML password attributes.
    SamlPasswordAttributes::DeleteFromPrefs(primary_profile->GetPrefs());
    return nullptr;
  }
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

void InSessionPasswordChangeManager::OnSamlPasswordChanged(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password) {
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), true);
  PasswordChangeDialog::Dismiss();

  const bool both_passwords_scraped =
      !scraped_old_password.empty() && !scraped_new_password.empty();
  if (both_passwords_scraped) {
    // Both passwords scraped so we try to change cryptohome password now.
    // Show the confirm dialog but initially showing a spinner. If the change
    // fails, then the dialog will hide the spinner and show a prompt.
    // If the change succeeds, the dialog and spinner will just disappear.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/true);
    ChangePassword(scraped_old_password, scraped_new_password);
  } else {
    // Failed to scrape passwords - prompt for passwords immediately.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/false);
  }
}

void InSessionPasswordChangeManager::ChangePassword(
    const std::string& old_password,
    const std::string& new_password) {
  UserContext user_context(*primary_user_);
  user_context.SetKey(Key(new_password));
  authenticator_->MigrateKey(user_context, old_password);
}

void InSessionPasswordChangeManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void InSessionPasswordChangeManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
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
  VLOG(1) << "Failed to change cryptohome password: " << error.GetErrorString();
  NotifyObservers(CHANGE_PASSWORD_AUTH_FAILURE);
}

void InSessionPasswordChangeManager::NotifyObservers(Event event) {
  for (auto& observer : observer_list_) {
    observer.OnEvent(event);
  }
}

}  // namespace chromeos
