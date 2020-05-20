// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/webui/web_ui_util.h"

SigninReauthHandler::SigninReauthHandler(Browser* browser) : browser_(browser) {
  DCHECK(browser_);
}

SigninReauthHandler::~SigninReauthHandler() = default;

void SigninReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SigninReauthHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel", base::BindRepeating(&SigninReauthHandler::HandleCancel,
                                    base::Unretained(this)));
}

void SigninReauthHandler::HandleConfirm(const base::ListValue* args) {
  browser_->signin_view_controller()->OnReauthConfirmed();
}

void SigninReauthHandler::HandleCancel(const base::ListValue* args) {
  browser_->signin_view_controller()->CloseModalSignin();
}
