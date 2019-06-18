// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/insession_password_change_handler_chromeos.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

InSessionPasswordChangeHandler::InSessionPasswordChangeHandler(
    const std::string& password_change_url)
    : password_change_url_(password_change_url) {}
InSessionPasswordChangeHandler::~InSessionPasswordChangeHandler() = default;

void InSessionPasswordChangeHandler::HandleInitialize(
    const base::ListValue* value) {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));

  AllowJavascript();
  base::Value params(base::Value::Type::DICTIONARY);
  if (password_change_url_.empty()) {
    LOG(ERROR) << "Password change url is empty";
    return;
  }
  params.SetKey("passwordChangeUrl", base::Value(password_change_url_));
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    params.SetKey("userName", base::Value(user->GetDisplayEmail()));
  CallJavascriptFunction("insession.password.change.loadAuthExtension", params);
}

void InSessionPasswordChangeHandler::HandleChangePassword(
    const base::ListValue* params) {
  const base::Value& old_passwords = params->GetList()[0];
  const base::Value& new_passwords = params->GetList()[1];
  VLOG(4) << "Scraped " << old_passwords.GetList().size() << " old passwords";
  VLOG(4) << "Scraped " << new_passwords.GetList().size() << " new passwords";
  user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(Profile::FromWebUI(web_ui()));
  user_manager::UserManager::Get()->SaveForceOnlineSignin(user->GetAccountId(),
                                                          true);
  if (new_passwords.GetList().size() == 1 &&
      old_passwords.GetList().size() > 0) {
    auto* in_session_password_change_manager =
        g_browser_process->platform_part()
            ->in_session_password_change_manager();
    CHECK(in_session_password_change_manager);
    in_session_password_change_manager->ChangePassword(
        old_passwords.GetList()[0].GetString(),
        new_passwords.GetList()[0].GetString());
  }

  // TODO(https://crbug.com/930109): Failed to scrape passwords - show dialog.
}

void InSessionPasswordChangeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&InSessionPasswordChangeHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "changePassword",
      base::BindRepeating(&InSessionPasswordChangeHandler::HandleChangePassword,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
