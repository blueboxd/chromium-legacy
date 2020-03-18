// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_reauth_popup_delegate.h"

#include "base/logging.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
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
  NavigateParams nav_params(browser_, reauth_url(),
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.trusted_source = false;
  nav_params.user_gesture = true;
  nav_params.window_bounds = gfx::Rect(kPopupWidth, kPopupHeight);

  Navigate(&nav_params);

  web_contents_ = nav_params.navigated_or_inserted_contents;
  content::WebContentsObserver::Observe(web_contents_);
}

SigninReauthPopupDelegate::~SigninReauthPopupDelegate() = default;

void SigninReauthPopupDelegate::CloseModalSignin() {
  CompleteReauth(signin::ReauthResult::kCancelled);
}

void SigninReauthPopupDelegate::ResizeNativeView(int height) {
  NOTIMPLEMENTED();
}

content::WebContents* SigninReauthPopupDelegate::GetWebContents() {
  return web_contents_;
}

void SigninReauthPopupDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL url_without_query =
      navigation_handle->GetURL().ReplaceComponents(replacements);
  if (url_without_query != reauth_url())
    return;

  // TODO(https://crbug.com/1045515): update the response code once Gaia
  // implements a landing page.
  if (navigation_handle->IsErrorPage() ||
      !navigation_handle->GetResponseHeaders() ||
      navigation_handle->GetResponseHeaders()->response_code() !=
          net::HTTP_NOT_IMPLEMENTED) {
    CompleteReauth(signin::ReauthResult::kLoadFailed);
    return;
  }

  CompleteReauth(signin::ReauthResult::kSuccess);
}

void SigninReauthPopupDelegate::WebContentsDestroyed() {
  if (signin_view_controller_) {
    signin_view_controller_->ResetModalSigninDelegate();
    signin_view_controller_ = nullptr;
  }
  // The last chance to invoke |reauth_callback_|. Run it only if WebContents
  // descruction was caused by an event outside of this class.
  if (reauth_callback_)
    std::move(reauth_callback_).Run(signin::ReauthResult::kDismissedByUser);
  delete this;
}

void SigninReauthPopupDelegate::CompleteReauth(signin::ReauthResult result) {
  std::move(reauth_callback_).Run(result);
  // Close WebContents asynchronously so other WebContentsObservers can safely
  // finish their task.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SigninReauthPopupDelegate::CloseWebContents,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SigninReauthPopupDelegate::CloseWebContents() {
  web_contents_->ClosePage();
}

const GURL& SigninReauthPopupDelegate::reauth_url() const {
  return GaiaUrls::GetInstance()->reauth_url();
}
