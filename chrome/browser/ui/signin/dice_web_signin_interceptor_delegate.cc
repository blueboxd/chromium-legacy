// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/ui/browser_finder.h"

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

  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  ShowSigninInterceptionBubbleInternal(
      chrome::FindBrowserWithWebContents(web_contents));
}
