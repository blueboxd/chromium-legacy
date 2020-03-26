// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_REAUTH_POPUP_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_REAUTH_POPUP_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class SigninViewController;
class Browser;
struct CoreAccountId;

namespace content {
class WebContents;
}

class SigninReauthPopupDelegate : public SigninViewControllerDelegate,
                                  public content::WebContentsObserver {
 public:
  SigninReauthPopupDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser,
      const CoreAccountId& account_id,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);
  ~SigninReauthPopupDelegate() override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  void CompleteReauth(signin::ReauthResult result);
  void CloseWebContents();

  SigninViewController* signin_view_controller_;
  Browser* const browser_;
  base::OnceCallback<void(signin::ReauthResult)> reauth_callback_;
  content::WebContents* web_contents_;

  base::WeakPtrFactory<SigninReauthPopupDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_REAUTH_POPUP_DELEGATE_H_
