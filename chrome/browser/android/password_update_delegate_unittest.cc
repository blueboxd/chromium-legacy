// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_update_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/password_update_delegate.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Pair;

namespace {

constexpr char kExampleCom[] = "https://example.com/";
constexpr char kExampleOrg[] = "https://example.org/";
constexpr char kNewPass[] = "new pass";
constexpr char kNewUser[] = "new user";
constexpr char kPassword[] = "pass";
constexpr char kPassword2[] = "pass2";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";

std::vector<std::pair<std::string, std::string>> GetUsernamesAndPasswords(
    const std::vector<autofill::PasswordForm>& forms) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(forms.size());
  for (const auto& form : forms) {
    result.emplace_back(base::UTF16ToUTF8(form.username_value),
                        base::UTF16ToUTF8(form.password_value));
  }

  return result;
}

autofill::PasswordForm MakeSavedForm(const GURL& origin,
                                     base::StringPiece username,
                                     base::StringPiece password) {
  autofill::PasswordForm form;
  form.origin = origin;
  form.signon_realm = origin.GetOrigin().spec();
  form.username_element = base::ASCIIToUTF16("Email");
  form.username_value = base::ASCIIToUTF16(username);
  form.password_element = base::ASCIIToUTF16("Passwd");
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

}  // namespace

class PasswordUpdateDelegateTest : public testing::Test {
 protected:
  PasswordUpdateDelegateTest() {}

  ~PasswordUpdateDelegateTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<PasswordUpdateDelegate> CreateTestDelegate(
      const GURL& origin,
      base::StringPiece username,
      base::StringPiece password) {
    return std::make_unique<PasswordUpdateDelegate>(
        &profile_, MakeSavedForm(origin, username, password));
  }

  const std::vector<autofill::PasswordForm>& GetStoredPasswordsForRealm(
      base::StringPiece signon_realm) {
    const auto& stored_passwords =
        static_cast<const password_manager::TestPasswordStore&>(*store_)
            .stored_passwords();
    auto for_realm_it = stored_passwords.find(signon_realm);
    return for_realm_it->second;
  }

  password_manager::PasswordStore* GetStore() { return store_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  scoped_refptr<password_manager::PasswordStore> store_ =
      base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
          PasswordStoreFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  &profile_,
                  base::BindRepeating(&password_manager::BuildPasswordStore<
                                      content::BrowserContext,
                                      password_manager::TestPasswordStore>))
              .get()));
};

TEST_F(PasswordUpdateDelegateTest, EditSavedPassword_EditPassword) {
  GetStore()->AddLogin(MakeSavedForm(GURL(kExampleCom), kUsername, kPassword));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername),
                                              base::ASCIIToUTF16(kNewPass));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kNewPass)));
}

TEST_F(PasswordUpdateDelegateTest, EditSavedPassword_EditUsername) {
  GetStore()->AddLogin(MakeSavedForm(GURL(kExampleCom), kUsername, kPassword));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                              base::ASCIIToUTF16(kPassword));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kPassword)));
}

TEST_F(PasswordUpdateDelegateTest, EditSavedPassword_EditUsernameAndPassword) {
  GetStore()->AddLogin(MakeSavedForm(GURL(kExampleCom), kUsername, kPassword));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                              base::ASCIIToUTF16(kNewPass));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kNewPass)));
}

TEST_F(PasswordUpdateDelegateTest,
       EditSavedPassword_RejectSameUsernameForSameRealm) {
  GetStore()->AddLogin(MakeSavedForm(GURL(kExampleCom), kUsername, kPassword));
  GetStore()->AddLogin(
      MakeSavedForm(GURL(kExampleCom), kUsername2, kPassword2));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername2),
                                              base::ASCIIToUTF16(kPassword));
  RunUntilIdle();
  EXPECT_THAT(
      GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
      ElementsAre(Pair(kUsername, kPassword), Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordUpdateDelegateTest,
       EditSavedPassword_DontRejectSameUsernameForDifferentRealm) {
  GetStore()->AddLogin(MakeSavedForm(GURL(kExampleCom), kUsername, kPassword));
  GetStore()->AddLogin(
      MakeSavedForm(GURL(kExampleOrg), kUsername2, kPassword2));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kUsername2),
                                              base::ASCIIToUTF16(kPassword2));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername2, kPassword2)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              ElementsAre(Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordUpdateDelegateTest, EditSavedPassword_UpdateDuplicates) {
  GetStore()->AddLogin(MakeSavedForm(GURL(base::StrCat({kExampleCom, "pathA"})),
                                     kUsername, kPassword));
  GetStore()->AddLogin(MakeSavedForm(GURL(base::StrCat({kExampleCom, "pathB"})),
                                     kUsername, kPassword));
  RunUntilIdle();
  std::unique_ptr<PasswordUpdateDelegate> password_update_delegate =
      CreateTestDelegate(GURL(kExampleCom), kUsername, kPassword);
  password_update_delegate->EditSavedPassword(base::ASCIIToUTF16(kNewUser),
                                              base::ASCIIToUTF16(kNewPass));
  RunUntilIdle();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kNewPass), Pair(kNewUser, kNewPass)));
}
