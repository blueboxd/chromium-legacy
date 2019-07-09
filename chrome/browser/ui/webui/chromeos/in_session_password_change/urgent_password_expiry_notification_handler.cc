// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/urgent_password_expiry_notification_handler.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

UrgentPasswordExpiryNotificationHandler::
    UrgentPasswordExpiryNotificationHandler() = default;

UrgentPasswordExpiryNotificationHandler::
    ~UrgentPasswordExpiryNotificationHandler() = default;

void UrgentPasswordExpiryNotificationHandler::HandleContinue(
    const base::ListValue* params) {
  auto* in_session_password_change_manager =
      g_browser_process->platform_part()->in_session_password_change_manager();
  CHECK(in_session_password_change_manager);
  in_session_password_change_manager->StartInSessionPasswordChange();
}

void UrgentPasswordExpiryNotificationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "continue", base::BindRepeating(
                      &UrgentPasswordExpiryNotificationHandler::HandleContinue,
                      weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
