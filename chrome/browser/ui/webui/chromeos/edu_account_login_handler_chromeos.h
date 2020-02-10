// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_ACCOUNT_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_ACCOUNT_LOGIN_HANDLER_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace chromeos {

// Handler for EDU account login flow.
class EduAccountLoginHandler : public content::WebUIMessageHandler,
                               public FamilyInfoFetcher::Consumer,
                               public GaiaAuthConsumer {
 public:
  explicit EduAccountLoginHandler(
      const base::RepeatingClosure& close_dialog_closure);
  ~EduAccountLoginHandler() override;
  EduAccountLoginHandler(const EduAccountLoginHandler&) = delete;
  EduAccountLoginHandler& operator=(const EduAccountLoginHandler&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest, HandleGetParentsSuccess);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest, HandleGetParentsFailure);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninSuccess);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninAccessTokenFailure);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninReAuthProofTokenFailure);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  void HandleGetParents(const base::ListValue* args);
  void HandleCloseDialog(const base::ListValue* args);
  void HandleParentSignin(const base::ListValue* args);

  virtual void FetchFamilyMembers();
  virtual void FetchAccessToken(const std::string& obfuscated_gaia_id,
                                const std::string& password);

  virtual void FetchReAuthProofTokenForParent(
      const std::string& child_oauth_access_token,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& parent_credential);

  // FamilyInfoFetcher::Consumer implementation.
  void OnGetFamilyMembersSuccess(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) override;
  void OnFailure(FamilyInfoFetcher::ErrorCode error) override;

  // signin::PrimaryAccountAccessTokenFetcher callback
  void CreateReAuthProofTokenForParent(
      const std::string& parent_obfuscated_gaia_id,
      const std::string& parent_credential,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // GaiaAuthConsumer overrides.
  void OnReAuthProofTokenSuccess(
      const std::string& reauth_proof_token) override;
  void OnReAuthProofTokenFailure(
      const GaiaAuthConsumer::ReAuthProofTokenStatus error) override;

  // Used for getting parent RAPT token.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  // Used for getting child access token.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::RepeatingClosure close_dialog_closure_;
  std::unique_ptr<FamilyInfoFetcher> family_fetcher_;
  std::string get_parents_callback_id_;
  std::string parent_signin_callback_id_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_ACCOUNT_LOGIN_HANDLER_CHROMEOS_H_
