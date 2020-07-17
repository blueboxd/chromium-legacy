// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include "base/callback.h"

namespace base {
class ListValue;
}

// WebUI message handler for the Dice web signin intercept bubble.
class DiceWebSigninInterceptHandler : public content::WebUIMessageHandler {
 public:
  explicit DiceWebSigninInterceptHandler(
      base::OnceCallback<void(bool)> callback);
  ~DiceWebSigninInterceptHandler() override;

  DiceWebSigninInterceptHandler(const DiceWebSigninInterceptHandler&) = delete;
  DiceWebSigninInterceptHandler& operator=(
      const DiceWebSigninInterceptHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleAccept(const base::ListValue* args);
  void HandleCancel(const base::ListValue* args);

  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
