// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include "base/callback_forward.h"

namespace content {
class WebContents;
}

struct AccountInfo;
class Browser;

class DiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  DiceWebSigninInterceptorDelegate();
  ~DiceWebSigninInterceptorDelegate() override;

  // DiceWebSigninInterceptor::Delegate
  void ShowSigninInterceptionBubble(
      DiceWebSigninInterceptor::SigninInterceptionType signin_interception_type,
      content::WebContents* web_contents,
      const AccountInfo& account_info,
      base::OnceCallback<void(bool)> callback) override;

 private:
  // Implemented in dice_web_signin_interception_bubble_view.cc
  void ShowSigninInterceptionBubbleInternal(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_DELEGATE_H_
