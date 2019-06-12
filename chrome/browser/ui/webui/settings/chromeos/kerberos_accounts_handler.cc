// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {

KerberosAccountsHandler::KerberosAccountsHandler()
    : credentials_manager_observer_(this), weak_factory_(this) {}

KerberosAccountsHandler::~KerberosAccountsHandler() = default;

void KerberosAccountsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getKerberosAccounts",
      base::BindRepeating(&KerberosAccountsHandler::HandleGetKerberosAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleAddKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleRemoveKerberosAccount,
                          weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "setAsActiveKerberosAccount",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleSetAsActiveKerberosAccount,
          weak_factory_.GetWeakPtr()));
}

void KerberosAccountsHandler::HandleGetKerberosAccounts(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  base::Value callback_id = args->GetList()[0].Clone();

  KerberosCredentialsManager::Get().ListAccounts(
      base::BindOnce(&KerberosAccountsHandler::OnListAccounts,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void KerberosAccountsHandler::OnListAccounts(
    base::Value callback_id,
    const kerberos::ListAccountsResponse& response) {
  base::ListValue accounts;

  // Default icon is a briefcase.
  gfx::ImageSkia skia_default_icon =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGIN_DEFAULT_USER_2);
  std::string default_icon = webui::GetBitmapDataUrl(
      skia_default_icon.GetRepresentation(1.0f).GetBitmap());

  const std::string& active_principal =
      KerberosCredentialsManager::Get().GetActiveAccount();

  for (int n = 0; n < response.accounts_size(); ++n) {
    const kerberos::Account& account = response.accounts(n);
    const bool is_active = account.principal_name() == active_principal;

    base::DictionaryValue account_dict;
    account_dict.SetString("principalName", account.principal_name());
    account_dict.SetString("krb5conf", account.krb5conf());
    account_dict.SetBoolean("isSignedIn", account.tgt_validity_seconds() > 0);
    account_dict.SetBoolean("isActive", is_active);
    account_dict.SetString("pic", default_icon);
    accounts.GetList().push_back(std::move(account_dict));
  }

  ResolveJavascriptCallback(callback_id, accounts);
}

void KerberosAccountsHandler::HandleAddKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  // TODO(https://crbug.com/961246):
  //   - Prevent account changes when Kerberos is disabled.
  //   - Remove all accounts when Kerberos is disabled.

  CHECK_EQ(3U, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& principal_name = args->GetList()[1].GetString();
  const std::string& password = args->GetList()[2].GetString();

  KerberosCredentialsManager::Get().AddAccountAndAuthenticate(
      std::move(principal_name), false /* is_managed */, password,
      false /* remember_password */, base::nullopt /* krb5_conf */,
      base::BindOnce(&KerberosAccountsHandler::OnAddAccountAndAuthenticate,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnAddAccountAndAuthenticate(
    const std::string& callback_id,
    kerberos::ErrorType error) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(error)));
}

void KerberosAccountsHandler::HandleRemoveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const std::string& principal_name = args->GetList()[0].GetString();

  // Note that we're observing the credentials manager, so OnAccountsChanged()
  // is called when an account is removed, which calls RefreshUI(). Thus, it's
  // fine to pass an EmptyResultCallback() in here and not something that calls
  // RefreshUI().
  KerberosCredentialsManager::Get().RemoveAccount(
      principal_name, KerberosCredentialsManager::EmptyResultCallback());
}

void KerberosAccountsHandler::HandleSetAsActiveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const std::string& principal_name = args->GetList()[0].GetString();

  KerberosCredentialsManager::Get().SetActiveAccount(principal_name);
}

void KerberosAccountsHandler::OnJavascriptAllowed() {
  credentials_manager_observer_.Add(&KerberosCredentialsManager::Get());
}

void KerberosAccountsHandler::OnJavascriptDisallowed() {
  credentials_manager_observer_.RemoveAll();
}

void KerberosAccountsHandler::OnAccountsChanged() {
  RefreshUI();
}

void KerberosAccountsHandler::RefreshUI() {
  FireWebUIListener("kerberos-accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
