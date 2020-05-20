// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

class Browser;

namespace base {
class ListValue;
}

// WebUI message handler for the signin reauth dialog.
class SigninReauthHandler : public content::WebUIMessageHandler {
 public:
  // Creates a SigninReauthHandler for the |browser|.
  explicit SigninReauthHandler(Browser* browser);
  ~SigninReauthHandler() override;

  SigninReauthHandler(const SigninReauthHandler&) = delete;
  SigninReauthHandler& operator=(const SigninReauthHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 protected:
  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete the
  // reauth flow.
  virtual void HandleConfirm(const base::ListValue* args);

  // Handles "cancel" message from the page. No arguments. This message is sent
  // when the user cancels the reauth flow.
  virtual void HandleCancel(const base::ListValue* args);

 private:
  Browser* const browser_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
