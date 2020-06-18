// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_reauth_view_controller.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/reauth_tab_helper.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

class ReauthWebContentsObserver : public content::WebContentsObserver {
 public:
  ReauthWebContentsObserver(content::WebContents* web_contents,
                            SigninReauthViewController* delegate);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  SigninReauthViewController* const delegate_;
};

ReauthWebContentsObserver::ReauthWebContentsObserver(
    content::WebContents* web_contents,
    SigninReauthViewController* delegate)
    : WebContentsObserver(web_contents), delegate_(delegate) {}

void ReauthWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  delegate_->OnGaiaReauthPageNavigated();
}

}  // namespace

SigninReauthViewController::SigninReauthViewController(
    Browser* browser,
    const CoreAccountId& account_id,
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback)
    : browser_(browser),
      account_id_(account_id),
      reauth_callback_(std::move(reauth_callback)) {
  // Show the confirmation dialog unconditionally for now. We may decide to only
  // show it in some cases in the future.
  ShowReauthConfirmationDialog();

  if (!base::FeatureList::IsEnabled(kSigninReauthPrompt)) {
    // Approve reauth automatically.
    gaia_reauth_page_state_ = GaiaReauthPageState::kDone;
    gaia_reauth_page_result_ = signin::ReauthResult::kSuccess;
    OnStateChanged();
    return;
  }

  // Navigate to the Gaia reauth challenge page in background.
  reauth_web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          browser_->profile(),
          content::SiteInstance::Create(browser_->profile())));
  const GURL& reauth_url = GaiaUrls::GetInstance()->reauth_url();
  reauth_web_contents_->GetController().LoadURL(
      reauth_url, content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  signin::ReauthTabHelper::CreateForWebContents(
      reauth_web_contents_.get(), reauth_url, false,
      base::BindOnce(&SigninReauthViewController::OnGaiaReauthPageComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  reauth_web_contents_observer_ = std::make_unique<ReauthWebContentsObserver>(
      reauth_web_contents_.get(), this);
}

SigninReauthViewController::~SigninReauthViewController() = default;

void SigninReauthViewController::CloseModalSignin() {
  CompleteReauth(signin::ReauthResult::kCancelled);
}

void SigninReauthViewController::ResizeNativeView(int height) {
  NOTIMPLEMENTED();
}

content::WebContents* SigninReauthViewController::GetWebContents() {
  // If the dialog is displayed, return its WebContents.
  if (dialog_delegate_)
    return dialog_delegate_->GetWebContents();

  // Return contents of the SAML flow, if exist.
  return raw_reauth_web_contents_;
}

void SigninReauthViewController::SetWebContents(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
}

void SigninReauthViewController::OnModalSigninClosed() {
  dialog_delegate_observer_.Remove(dialog_delegate_);
  dialog_delegate_ = nullptr;
  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

void SigninReauthViewController::OnReauthConfirmed() {
  if (user_confirmed_reauth_)
    return;

  user_confirmed_reauth_ = true;
  OnStateChanged();
}

void SigninReauthViewController::OnReauthDismissed() {
  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

void SigninReauthViewController::OnGaiaReauthPageNavigated() {
  if (gaia_reauth_page_state_ >= GaiaReauthPageState::kNavigated)
    return;

  gaia_reauth_page_state_ = GaiaReauthPageState::kNavigated;
  OnStateChanged();
}

void SigninReauthViewController::OnGaiaReauthPageComplete(
    signin::ReauthResult result) {
  // Should be called only once.
  DCHECK(gaia_reauth_page_state_ < GaiaReauthPageState::kDone);
  DCHECK(!gaia_reauth_page_result_);
  gaia_reauth_page_state_ = GaiaReauthPageState::kDone;
  gaia_reauth_page_result_ = result;
  OnStateChanged();
}

void SigninReauthViewController::SetObserverForTesting(
    Observer* test_observer) {
  test_observer_ = test_observer;
}

void SigninReauthViewController::CompleteReauth(signin::ReauthResult result) {
  if (dialog_delegate_) {
    dialog_delegate_observer_.Remove(dialog_delegate_);
    dialog_delegate_->CloseModalSignin();
    dialog_delegate_ = nullptr;
  }

  if (raw_reauth_web_contents_) {
    if (!raw_reauth_web_contents_->IsBeingDestroyed())
      raw_reauth_web_contents_->ClosePage();
    raw_reauth_web_contents_ = nullptr;
  }

  if (reauth_callback_)
    std::move(reauth_callback_).Run(result);

  // NotifyModalSigninClosed() will destroy |this|. But since this function can
  // be triggered from |reauth_web_contents_|'s observer method, we cannot
  // destroy |reauth_web_contents_| right now.
  content::GetUIThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, reauth_web_contents_.release());
  NotifyModalSigninClosed();
}

void SigninReauthViewController::OnStateChanged() {
  if (user_confirmed_reauth_ &&
      gaia_reauth_page_state_ == GaiaReauthPageState::kNavigated) {
    ShowGaiaReauthPage();
    return;
  }

  if (user_confirmed_reauth_ &&
      gaia_reauth_page_state_ == GaiaReauthPageState::kDone) {
    DCHECK(gaia_reauth_page_result_);
    // TODO(https://crbug.com/1085026): display an error if the page load has
    // failed.
    CompleteReauth(*gaia_reauth_page_result_);
    return;
  }
}

void SigninReauthViewController::ShowReauthConfirmationDialog() {
  dialog_delegate_ =
      SigninViewControllerDelegate::CreateReauthConfirmationDelegate(
          browser_, account_id_);
  dialog_delegate_observer_.Add(dialog_delegate_);
}

void SigninReauthViewController::ShowGaiaReauthPage() {
  signin::ReauthTabHelper* tab_helper =
      signin::ReauthTabHelper::FromWebContents(reauth_web_contents_.get());
  DCHECK(tab_helper);

  if (tab_helper->is_within_reauth_origin()) {
    ShowGaiaReauthPageInDialog();
  } else {
    // This corresponds to a SAML account.
    ShowGaiaReauthPageInNewTab();
  }

  if (test_observer_)
    test_observer_->OnGaiaReauthPageShown();
}

void SigninReauthViewController::ShowGaiaReauthPageInDialog() {
  dialog_delegate_->SetWebContents(reauth_web_contents_.get());
}

void SigninReauthViewController::ShowGaiaReauthPageInNewTab() {
  // Remove the observer to not trigger OnModalSigninClosed() that will abort
  // the reauth flow.
  dialog_delegate_observer_.Remove(dialog_delegate_);
  dialog_delegate_->CloseModalSignin();
  dialog_delegate_ = nullptr;

  raw_reauth_web_contents_ = reauth_web_contents_.get();
  NavigateParams nav_params(browser_, std::move(reauth_web_contents_));
  nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.trusted_source = false;
  nav_params.user_gesture = true;
  nav_params.tabstrip_add_types |= TabStripModel::ADD_INHERIT_OPENER;
  Navigate(&nav_params);
}
