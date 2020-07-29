// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

DiceWebSigninInterceptHandler::DiceWebSigninInterceptHandler(
    const AccountInfo& account_info,
    base::OnceCallback<void(bool)> callback)
    : account_info_(account_info), callback_(std::move(callback)) {
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
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandlePageLoaded,
                          base::Unretained(this)));
}

void DiceWebSigninInterceptHandler::OnJavascriptAllowed() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  identity_observer_.Add(identity_manager);
}

void DiceWebSigninInterceptHandler::OnJavascriptDisallowed() {
  identity_observer_.RemoveAll();
}

void DiceWebSigninInterceptHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id == account_info_.account_id) {
    account_info_ = info;
    FireWebUIListener("account-info-changed", GetAccountInfoValue());
  }
}

void DiceWebSigninInterceptHandler::HandleAccept(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(true);
}

void DiceWebSigninInterceptHandler::HandleCancel(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(false);
}

void DiceWebSigninInterceptHandler::HandlePageLoaded(
    const base::ListValue* args) {
  AllowJavascript();

  // Update the account info and the image.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  base::Optional<AccountInfo> info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          account_info_);
  if (info)
    account_info_ = info.value();

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, GetAccountInfoValue());
}

base::Value DiceWebSigninInterceptHandler::GetAccountInfoValue() {
  std::string picture_url_to_load =
      account_info_.account_image.IsEmpty()
          ? profiles::GetPlaceholderAvatarIconUrl()
          : webui::GetBitmapDataUrl(account_info_.account_image.AsBitmap());
  base::Value account_info_value(base::Value::Type::DICTIONARY);
  account_info_value.SetStringKey("pictureUrl", picture_url_to_load);
  account_info_value.SetStringKey("name", account_info_.given_name);
  return account_info_value;
}
