// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client_helper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using autofill::PasswordForm;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;

constexpr char kTestUsername[] = "test@gmail.com";
constexpr char kTestPassword[] = "T3stP@$$w0rd";
constexpr char kTestOrigin[] = "https://example.com/";

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(void,
              PromptUserToMovePasswordToAccount,
              (std::unique_ptr<PasswordFormManagerForUI>),
              (override));
  MOCK_METHOD(void, PromptUserToEnableAutosignin, (), (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(bool, IsIncognito, (), (const, override));
};

PasswordForm CreateForm(std::string username,
                        std::string password,
                        GURL origin) {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.origin = origin;
  form.signon_realm = origin.spec();
  return form;
}

std::unique_ptr<PasswordFormManagerForUI> CreateFormManager(
    const PasswordForm* form,
    bool is_movable) {
  auto manager = std::make_unique<NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*manager, GetPendingCredentials)
      .WillByDefault(testing::ReturnRef(*form));
  ON_CALL(*manager, IsMovableToAccountStore).WillByDefault(Return(is_movable));
  return manager;
}

}  // namespace

class PasswordManagerClientHelperTest : public testing::Test {
 public:
  PasswordManagerClientHelperTest() : helper_(&client_) {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableAutosignin,
                                           true);
    prefs_.SetBoolean(prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.SetBoolean(prefs::kCredentialsEnableAutosignin, true);
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(&prefs_));
  }
  ~PasswordManagerClientHelperTest() override = default;

 protected:
  PasswordManagerClientHelper* helper() { return &helper_; }
  MockPasswordManagerClient* client() { return &client_; }

  NiceMock<MockPasswordManagerClient> client_;
  PasswordManagerClientHelper helper_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordManagerClientHelperTest, PromptAutosigninAfterSuccessfulLogin) {
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);

  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifyUserCouldBeAutoSignedIn(std::make_unique<PasswordForm>(form));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, PromptAutosigninDisabledInIncognito) {
  EXPECT_CALL(*client(), IsIncognito)
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);

  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifyUserCouldBeAutoSignedIn(std::make_unique<PasswordForm>(form));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, PromptMoveForMovableForm) {
  base::test::ScopedFeatureList account_storage_feature;
  account_storage_feature.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount);
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);

  // Indicate successful login without matching form.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, NoPromptToMoveWithoutFeature) {
  base::test::ScopedFeatureList account_storage_feature;
  account_storage_feature.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);

  // Indicate successful login without matching form.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, NoPromptToMoveForUnmovableForm) {
  base::test::ScopedFeatureList account_storage_feature;
  account_storage_feature.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);

  // Indicate successful login without matching form.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/false));
}

}  // namespace password_manager
