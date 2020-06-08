// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace {

// TODO(https://crbug.com/1076880): Delete this class once the real interception
// UI is implemented.
class EnterpriseConfirmationDialogDelegate
    : public ui::ProfileSigninConfirmationDelegate {
 public:
  explicit EnterpriseConfirmationDialogDelegate(
      base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {}

  ~EnterpriseConfirmationDialogDelegate() override = default;

  void OnCancelSignin() override {
    // Cancelling signin won't be supported with the real UI.
    NOTIMPLEMENTED();
    OnContinueSignin();
  }

  void OnContinueSignin() override { std::move(callback_).Run(false); }

  void OnSigninWithNewProfile() override { std::move(callback_).Run(true); }

 private:
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

DiceWebSigninInterceptorDelegate::DiceWebSigninInterceptorDelegate() = default;

DiceWebSigninInterceptorDelegate::~DiceWebSigninInterceptorDelegate() = default;

void DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble(
    DiceWebSigninInterceptor::SigninInterceptionType signin_interception_type,
    content::WebContents* web_contents,
    const AccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  if (signin_interception_type !=
      DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise) {
    // Only the enterprise interception is currently implemented.
    std::move(callback).Run(false);
    return;
  }

  // TODO(https://crbug.com/1076880): Implement the interception UI. In the
  // meantime, the enterprise sync confirmation prompt is shown instead.
  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  TabDialogs::FromWebContents(web_contents)
      ->ShowProfileSigninConfirmation(
          chrome::FindBrowserWithWebContents(web_contents),
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          account_info.email,
          std::make_unique<EnterpriseConfirmationDialogDelegate>(
              std::move(callback)));
}
