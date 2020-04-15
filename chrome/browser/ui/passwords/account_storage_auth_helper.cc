// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"

namespace {
using ReauthSucceeded =
    password_manager::PasswordManagerClient::ReauthSucceeded;
}

AccountStorageAuthHelper::AccountStorageAuthHelper(Profile* profile)
    : profile_(profile) {}

void AccountStorageAuthHelper::TriggerOptInReauth(
    const CoreAccountId& account_id,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }
  SigninViewController* signin_view_controller =
      browser->signin_view_controller();
  if (!signin_view_controller) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }
  signin_view_controller->ShowReauthPrompt(
      account_id,
      base::BindOnce(
          [](base::OnceCallback<void(ReauthSucceeded)> reauth_callback,
             signin::ReauthResult result) {
            std::move(reauth_callback)
                .Run(ReauthSucceeded(result == signin::ReauthResult::kSuccess));
          },
          std::move(reauth_callback)));
}

void AccountStorageAuthHelper::TriggerSignIn() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser)
    return;
  if (SigninViewController* signin_controller =
          browser->signin_view_controller()) {
    signin_controller->ShowDiceAddAccountTab(
        signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN,
        std::string());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}
