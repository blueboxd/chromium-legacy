// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"

#include "base/bind.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/webui/web_ui_util.h"

SigninReauthHandler::SigninReauthHandler(SigninReauthViewController* controller)
    : controller_(controller) {
  DCHECK(controller_);
  controller_observer_.Add(controller_);
}

SigninReauthHandler::~SigninReauthHandler() = default;

void SigninReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&SigninReauthHandler::HandleInitialize,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SigninReauthHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel", base::BindRepeating(&SigninReauthHandler::HandleCancel,
                                    base::Unretained(this)));
}

void SigninReauthHandler::OnJavascriptAllowed() {
  if (!controller_)
    return;

  SigninReauthViewController::GaiaReauthType gaia_reauth_type =
      controller_->gaia_reauth_type();
  if (gaia_reauth_type !=
      SigninReauthViewController::GaiaReauthType::kUnknown) {
    OnGaiaReauthTypeDetermined(gaia_reauth_type);
  }
}

void SigninReauthHandler::OnReauthControllerDestroyed() {
  controller_observer_.RemoveAll();
  controller_ = nullptr;
}

void SigninReauthHandler::OnGaiaReauthTypeDetermined(
    SigninReauthViewController::GaiaReauthType reauth_type) {
  if (!IsJavascriptAllowed())
    return;

  DCHECK_NE(reauth_type, SigninReauthViewController::GaiaReauthType::kUnknown);
  bool is_reauth_required =
      reauth_type != SigninReauthViewController::GaiaReauthType::kAutoApproved;
  FireWebUIListener("reauth-type-received", base::Value(is_reauth_required));
}

void SigninReauthHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
}

void SigninReauthHandler::HandleConfirm(const base::ListValue* args) {
  if (controller_)
    controller_->OnReauthConfirmed();
}

void SigninReauthHandler::HandleCancel(const base::ListValue* args) {
  if (controller_)
    controller_->OnReauthDismissed();
}
