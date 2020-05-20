// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_

#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"

class Browser;

namespace content {
class WebUI;
}

// WebUI controller for the signin reauth dialog.
//
// The reauth UI currently assumes that the unconsented primary account matches
// the first account in cookies.
// It's a safe assumption only under the following conditions:
// - DICE is enabled
// - Sync in not enabled
//
// Currently this dialog is only used for account password storage opt-in that
// satisfies both of those conditions.
//
// Contact chrome-signin@chromium.org if you want to reuse this dialog for other
// reauth use-cases.
class SigninReauthUI : public SigninWebDialogUI {
 public:
  explicit SigninReauthUI(content::WebUI* web_ui);
  ~SigninReauthUI() override;

  SigninReauthUI(const SigninReauthUI&) = delete;
  SigninReauthUI& operator=(const SigninReauthUI&) = delete;

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_UI_H_
