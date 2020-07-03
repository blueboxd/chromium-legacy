// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_

#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// WebUI message handler for the signin reauth dialog.
class SigninReauthHandler : public content::WebUIMessageHandler,
                            public SigninReauthViewController::Observer {
 public:
  // Creates a SigninReauthHandler for the |controller|.
  explicit SigninReauthHandler(SigninReauthViewController* controller);
  ~SigninReauthHandler() override;

  SigninReauthHandler(const SigninReauthHandler&) = delete;
  SigninReauthHandler& operator=(const SigninReauthHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;

  // SigninReauthViewController::Observer:
  void OnReauthControllerDestroyed() override;
  void OnGaiaReauthTypeDetermined(
      SigninReauthViewController::GaiaReauthType reauth_type) override;

 protected:
  // Handles "initialize" message from the page. No arguments.
  virtual void HandleInitialize(const base::ListValue* args);

  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete the
  // reauth flow.
  virtual void HandleConfirm(const base::ListValue* args);

  // Handles "cancel" message from the page. No arguments. This message is sent
  // when the user cancels the reauth flow.
  virtual void HandleCancel(const base::ListValue* args);

 private:
  // May be null if |controller_| gets destroyed earlier than |this|.
  SigninReauthViewController* controller_;

  ScopedObserver<SigninReauthViewController,
                 SigninReauthViewController::Observer>
      controller_observer_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
