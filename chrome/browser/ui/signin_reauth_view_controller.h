// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_REAUTH_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_REAUTH_VIEW_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"

class Browser;

namespace {
class ReauthWebContentsObserver;
}

namespace content {
class WebContents;
}

namespace signin {
enum class ReauthResult;
}

// A controller class for the Reauth UI flow.
//
// The reauth flow consists of:
// - Reauth confirmation webUI page. Displayed in a tab-modal dialog.
// - Gaia Reauth page. Loaded from the web. Displayed either in a tab-modal
// dialog or in a new tab if an account requires SAML authentication. May be
// approved automatically. In that case, no UI is displayed to the user.
//
// The Gaia reauth page is loaded in background and gets shown to the user only
// after the user confirms the reauth confirmation dialog.
class SigninReauthViewController
    : public SigninViewControllerDelegate,
      public SigninViewControllerDelegate::Observer {
 public:
  // An observer class currently used only for tests.
  class Observer {
   public:
    virtual ~Observer() = default;
    // Called when the WebContents displaying the reauth confirmation UI has
    // been swapped with Gaia reauth WebContents.
    virtual void OnGaiaReauthPageShown() = 0;
  };

  enum class GaiaReauthPageState {
    kStarted = 0,    // The Gaia Reauth page is loading in background.
    kNavigated = 1,  // The first navigation has been committed.
    kDone = 2  // The reauth has been completed and the result is available.
  };

  SigninReauthViewController(
      Browser* browser,
      const CoreAccountId& account_id,
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);

  SigninReauthViewController(const SigninReauthViewController&) = delete;
  SigninReauthViewController& operator=(const SigninReauthViewController&) =
      delete;

  ~SigninReauthViewController() override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // SigninViewControllerDelegate::Observer:
  void OnModalSigninClosed() override;

  // Called when the user clicks the confirm button in the reauth confirmation
  // dialog.
  // This happens before the Gaia reauth page is shown.
  void OnReauthConfirmed();
  // Called when the user clicks the cancel button in the reauth confirmation
  // dialog.
  // This happens before the Gaia reauth page is shown.
  void OnReauthDismissed();

  // Called when the Gaia reauth page has navigated.
  void OnGaiaReauthPageNavigated();
  // Called when the Gaia reauth has been completed and the result is available.
  void OnGaiaReauthPageComplete(signin::ReauthResult result);

  // Public for testing.
  void SetObserverForTesting(Observer* test_observer);

 private:
  // Calls |reauth_callback_| with |result| and closes all Reauth UIs.
  void CompleteReauth(signin::ReauthResult result);

  // Notifies about a change in the reauth flow state.
  void OnStateChanged();

  void ShowReauthConfirmationDialog();
  void ShowGaiaReauthPage();
  void ShowGaiaReauthPageInDialog();
  void ShowGaiaReauthPageInNewTab();

  Browser* const browser_;
  const CoreAccountId account_id_;
  base::OnceCallback<void(signin::ReauthResult)> reauth_callback_;

  // Delegate displaying the dialog.
  SigninViewControllerDelegate* dialog_delegate_ = nullptr;
  ScopedObserver<SigninViewControllerDelegate,
                 SigninViewControllerDelegate::Observer>
      dialog_delegate_observer_{this};

  // WebContents of the Gaia reauth page.
  std::unique_ptr<content::WebContents> reauth_web_contents_;
  std::unique_ptr<ReauthWebContentsObserver> reauth_web_contents_observer_;
  // Raw pointer is only set if |reauth_web_contents_| was transferred to a new
  // tab for the SAML flow.
  content::WebContents* raw_reauth_web_contents_ = nullptr;

  // The state of the reauth flow.
  bool user_confirmed_reauth_ = false;
  GaiaReauthPageState gaia_reauth_page_state_ = GaiaReauthPageState::kStarted;
  base::Optional<signin::ReauthResult> gaia_reauth_page_result_;

  Observer* test_observer_ = nullptr;

  base::WeakPtrFactory<SigninReauthViewController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_REAUTH_VIEW_CONTROLLER_H_
