// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_reauth_popup_delegate.h"

#include "base/notreached.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/reauth_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

const int kPopupWidth = 657;
const int kPopupHeight = 708;

}  // namespace

SigninReauthPopupDelegate::SigninReauthPopupDelegate(
    SigninViewController* signin_view_controller,
    Browser* browser,
    const CoreAccountId& account_id,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback)
    : signin_view_controller_(signin_view_controller),
      browser_(browser),
      reauth_callback_(std::move(reauth_callback)) {
  const GURL& reauth_url = GaiaUrls::GetInstance()->reauth_url();
  NavigateParams nav_params(browser_, reauth_url,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.trusted_source = false;
  nav_params.user_gesture = true;
  nav_params.window_bounds = gfx::Rect(kPopupWidth, kPopupHeight);

  Navigate(&nav_params);

  web_contents_ = nav_params.navigated_or_inserted_contents;

  signin::ReauthTabHelper::CreateForWebContents(
      web_contents_, reauth_url, false,
      base::BindOnce(&SigninReauthPopupDelegate::CompleteReauth,
                     weak_ptr_factory_.GetWeakPtr()));
  content::WebContentsObserver::Observe(web_contents_);
}

SigninReauthPopupDelegate::~SigninReauthPopupDelegate() {
  // Last chance to invoke |reauth_callback_| if this hasn't been done before.
  if (reauth_callback_)
    std::move(reauth_callback_).Run(signin::ReauthResult::kDismissedByUser);
}

void SigninReauthPopupDelegate::CloseModalSignin() {
  CompleteReauth(signin::ReauthResult::kCancelled);
}

void SigninReauthPopupDelegate::ResizeNativeView(int height) {
  NOTIMPLEMENTED();
}

content::WebContents* SigninReauthPopupDelegate::GetWebContents() {
  return web_contents_;
}

void SigninReauthPopupDelegate::WebContentsDestroyed() {
  if (signin_view_controller_) {
    signin_view_controller_->ResetModalSigninDelegate();
    signin_view_controller_ = nullptr;
  }
  delete this;
}

void SigninReauthPopupDelegate::CompleteReauth(signin::ReauthResult result) {
  if (reauth_callback_)
    std::move(reauth_callback_).Run(result);

  if (!web_contents_->IsBeingDestroyed()) {
    // Close WebContents asynchronously so other WebContentsObservers can safely
    // finish their task.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SigninReauthPopupDelegate::CloseWebContents,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void SigninReauthPopupDelegate::CloseWebContents() {
  web_contents_->ClosePage();
}
