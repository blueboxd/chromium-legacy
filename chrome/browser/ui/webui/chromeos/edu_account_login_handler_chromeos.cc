// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/edu_account_login_handler_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"

namespace chromeos {

EduAccountLoginHandler::EduAccountLoginHandler(
    const base::RepeatingClosure& close_dialog_closure)
    : close_dialog_closure_(close_dialog_closure) {}

EduAccountLoginHandler::~EduAccountLoginHandler() {
  close_dialog_closure_.Run();
}

void EduAccountLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getParents",
      base::BindRepeating(&EduAccountLoginHandler::HandleGetParents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "parentSignin",
      base::BindRepeating(&EduAccountLoginHandler::HandleParentSignin,
                          base::Unretained(this)));
}

void EduAccountLoginHandler::OnJavascriptDisallowed() {
  family_fetcher_.reset();
  access_token_fetcher_.reset();
  gaia_auth_fetcher_.reset();
  get_parents_callback_id_.clear();
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::HandleGetParents(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(args->GetList().size(), 1u);

  if (!get_parents_callback_id_.empty()) {
    // HandleGetParents call is already in progress, reject the callback.
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }
  get_parents_callback_id_ = args->GetList()[0].GetString();

  FetchFamilyMembers();
}

void EduAccountLoginHandler::HandleParentSignin(const base::ListValue* args) {
  const base::Value::ConstListView& args_list = args->GetList();
  CHECK_EQ(args_list.size(), 3u);
  CHECK(args_list[0].is_string());

  if (!parent_signin_callback_id_.empty()) {
    // HandleParentSignin call is already in progress, reject the callback.
    RejectJavascriptCallback(args_list[0], base::Value());
    return;
  }
  parent_signin_callback_id_ = args_list[0].GetString();

  const base::DictionaryValue* parent = nullptr;
  args_list[1].GetAsDictionary(&parent);
  CHECK(parent);
  const base::Value* obfuscated_gaia_id_value =
      parent->FindKey("obfuscatedGaiaId");
  DCHECK(obfuscated_gaia_id_value);
  std::string obfuscated_gaia_id = obfuscated_gaia_id_value->GetString();

  std::string password;
  args_list[2].GetAsString(&password);

  FetchAccessToken(obfuscated_gaia_id, password);
}

void EduAccountLoginHandler::FetchFamilyMembers() {
  DCHECK(!family_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  family_fetcher_ = std::make_unique<FamilyInfoFetcher>(
      this, IdentityManagerFactory::GetForProfile(profile),
      account_manager->GetUrlLoaderFactory());
  family_fetcher_->StartGetFamilyMembers();
}

void EduAccountLoginHandler::FetchAccessToken(
    const std::string& obfuscated_gaia_id,
    const std::string& password) {
  DCHECK(!access_token_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "EduAccountLoginHandler", identity_manager, scopes,
          base::BindOnce(
              &EduAccountLoginHandler::CreateReAuthProofTokenForParent,
              base::Unretained(this), std::move(obfuscated_gaia_id),
              std::move(password)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void EduAccountLoginHandler::FetchReAuthProofTokenForParent(
    const std::string& child_oauth_access_token,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  DCHECK(!gaia_auth_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  gaia_auth_fetcher_ = std::make_unique<GaiaAuthFetcher>(
      this, gaia::GaiaSource::kChrome, account_manager->GetUrlLoaderFactory());
  gaia_auth_fetcher_->StartCreateReAuthProofTokenForParent(
      child_oauth_access_token, parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnGetFamilyMembersSuccess(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  family_fetcher_.reset();
  base::ListValue parents;

  for (const auto& member : members) {
    if (member.role != FamilyInfoFetcher::HEAD_OF_HOUSEHOLD &&
        member.role != FamilyInfoFetcher::PARENT) {
      continue;
    }

    base::DictionaryValue parent;
    parent.SetStringKey("email", member.email);
    parent.SetStringKey("displayName", member.display_name);
    parent.SetStringKey("profileImageUrl", member.profile_image_url);
    parent.SetStringKey("obfuscatedGaiaId", member.obfuscated_gaia_id);

    parents.Append(std::move(parent));
  }
  ResolveJavascriptCallback(base::Value(get_parents_callback_id_), parents);
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::OnFailure(FamilyInfoFetcher::ErrorCode error) {
  family_fetcher_.reset();
  RejectJavascriptCallback(base::Value(get_parents_callback_id_),
                           base::ListValue());
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::CreateReAuthProofTokenForParent(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR)
        << "Could not get access token to create ReAuthProofToken for parent"
        << error.ToString();
    base::DictionaryValue result;
    result.SetBoolKey("wrongPassword", false);
    RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
    parent_signin_callback_id_.clear();
    return;
  }

  FetchReAuthProofTokenForParent(access_token_info.token,
                                 parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnReAuthProofTokenSuccess(
    const std::string& reauth_proof_token) {
  gaia_auth_fetcher_.reset();
  ResolveJavascriptCallback(base::Value(parent_signin_callback_id_),
                            base::Value(reauth_proof_token));
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::OnReAuthProofTokenFailure(
    const GaiaAuthConsumer::ReAuthProofTokenStatus error) {
  LOG(ERROR) << "Failed to fetch ReAuthProofToken for the parent, error="
             << static_cast<int>(error);
  gaia_auth_fetcher_.reset();

  base::DictionaryValue result;
  result.SetBoolKey(
      "wrongPassword",
      error == GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
  parent_signin_callback_id_.clear();
}

}  // namespace chromeos
