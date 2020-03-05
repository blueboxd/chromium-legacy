// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/edu_account_login_handler_chromeos.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_web_ui.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kFakeParentGaiaId[] = "someObfuscatedGaiaId";
constexpr char kFakeParentCredential[] = "someParentCredential";
constexpr char kFakeAccessToken[] = "someAccessToken";

std::vector<FamilyInfoFetcher::FamilyMember> GetFakeFamilyMembers() {
  std::vector<FamilyInfoFetcher::FamilyMember> members;
  members.push_back(FamilyInfoFetcher::FamilyMember(
      kFakeParentGaiaId, FamilyInfoFetcher::HEAD_OF_HOUSEHOLD, "Homer Simpson",
      "homer@simpson.com", "http://profile.url/homer",
      "http://profile.url/homer/image"));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "anotherObfuscatedGaiaId", FamilyInfoFetcher::PARENT, "Marge Simpson",
      std::string(), "http://profile.url/marge", std::string()));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId3", FamilyInfoFetcher::CHILD, "Lisa Simpson",
      "lisa@gmail.com", std::string(), "http://profile.url/lisa/image"));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId4", FamilyInfoFetcher::CHILD, "Bart Simpson",
      "bart@bart.bart", std::string(), std::string()));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId5", FamilyInfoFetcher::MEMBER, std::string(),
      std::string(), std::string(), std::string()));
  return members;
}

base::DictionaryValue GetFakeParent() {
  base::DictionaryValue parent;
  parent.SetStringKey("email", "homer@simpson.com");
  parent.SetStringKey("displayName", "Homer Simpson");
  parent.SetStringKey("profileImageUrl", "http://profile.url/homer/image");
  parent.SetStringKey("obfuscatedGaiaId", kFakeParentGaiaId);
  return parent;
}

base::ListValue GetFakeParentsListValue() {
  base::ListValue parents;
  parents.Append(GetFakeParent());

  base::DictionaryValue parent2;
  parent2.SetStringKey("email", std::string());
  parent2.SetStringKey("displayName", "Marge Simpson");
  parent2.SetStringKey("profileImageUrl", std::string());
  parent2.SetStringKey("obfuscatedGaiaId", "anotherObfuscatedGaiaId");
  parents.Append(std::move(parent2));

  return parents;
}

class MockEduAccountLoginHandler : public EduAccountLoginHandler {
 public:
  explicit MockEduAccountLoginHandler(
      const base::RepeatingClosure& close_dialog_closure)
      : EduAccountLoginHandler(close_dialog_closure) {}
  using EduAccountLoginHandler::set_web_ui;

  MOCK_METHOD(void, FetchFamilyMembers, (), (override));
  MOCK_METHOD(void,
              FetchAccessToken,
              (const std::string& obfuscated_gaia_id,
               const std::string& password),
              (override));
  MOCK_METHOD(void,
              FetchReAuthProofTokenForParent,
              (const std::string& child_oauth_access_token,
               const std::string& parent_obfuscated_gaia_id,
               const std::string& parent_credential),
              (override));
};
}  // namespace

class EduAccountLoginHandlerTest : public testing::Test {
 public:
  EduAccountLoginHandlerTest() {}

  void SetUp() override {
    handler_ = std::make_unique<MockEduAccountLoginHandler>(base::DoNothing());
    handler_->set_web_ui(web_ui());
  }

  void VerifyJavascriptCallbackResolved(
      const content::TestWebUI::CallData& data,
      const std::string& event_name,
      bool success = true) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(event_name, callback_id);

    bool callback_success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&callback_success));
    EXPECT_EQ(success, callback_success);
  }

  MockEduAccountLoginHandler* handler() const { return handler_.get(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  std::unique_ptr<MockEduAccountLoginHandler> handler_;
  content::TestWebUI web_ui_;
};

TEST_F(EduAccountLoginHandlerTest, HandleGetParentsSuccess) {
  constexpr char callback_id[] = "handle-get-parents-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(&list_args);

  // Simulate successful fetching of family members.
  handler()->OnGetFamilyMembersSuccess(GetFakeFamilyMembers());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_EQ(GetFakeParentsListValue(), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleGetParentsFailure) {
  constexpr char callback_id[] = "handle-get-parents-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(&list_args);

  // Simulate failed fetching of family members.
  handler()->OnFailure(FamilyInfoFetcher::ErrorCode::NETWORK_ERROR);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false /*success*/);

  ASSERT_EQ(base::ListValue(), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninSuccess) {
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::TimeDelta::FromHours(1),
                              "id_token"));

  constexpr char fake_rapt[] = "fakeReauthProofToken";
  // Simulate successful fetching of ReAuthProofToken.
  handler()->OnReAuthProofTokenSuccess(fake_rapt);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_EQ(base::Value(fake_rapt), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninAccessTokenFailure) {
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR),
      signin::AccessTokenInfo());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false /*success*/);

  base::DictionaryValue result;
  result.SetBoolKey("wrongPassword", false);
  ASSERT_EQ(result, *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninReAuthProofTokenFailure) {
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::TimeDelta::FromHours(1),
                              "id_token"));

  // Simulate failed fetching of ReAuthProofToken.
  handler()->OnReAuthProofTokenFailure(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false);

  base::DictionaryValue result;
  result.SetBoolKey("wrongPassword", true);
  ASSERT_EQ(result, *data.arg3());
}

}  // namespace chromeos
