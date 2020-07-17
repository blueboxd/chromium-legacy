// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include "base/bind.h"
#include "content/public/browser/web_ui.h"

DiceWebSigninInterceptHandler::DiceWebSigninInterceptHandler(
    base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

DiceWebSigninInterceptHandler::~DiceWebSigninInterceptHandler() = default;

void DiceWebSigninInterceptHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "accept",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandleAccept,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandleCancel,
                          base::Unretained(this)));
}

void DiceWebSigninInterceptHandler::HandleAccept(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(true);
}

void DiceWebSigninInterceptHandler::HandleCancel(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(false);
}
