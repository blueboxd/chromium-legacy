// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "f00b4r";
constexpr char kPassword2[] = "s3cr3t";

using autofill::PasswordForm;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

struct MockCompromisedCredentialsProviderObserver
    : CompromisedCredentialsProvider::Observer {
  MOCK_METHOD(void,
              OnCompromisedCredentialsChanged,
              (CompromisedCredentialsProvider::CredentialsView),
              (override));
};

using StrictMockCompromisedCredentialsProviderObserver =
    ::testing::StrictMock<MockCompromisedCredentialsProviderObserver>;

CompromisedCredentials MakeCompromised(
    base::StringPiece signon_realm,
    base::StringPiece username,
    CompromiseType type = CompromiseType::kLeaked) {
  return {.signon_realm = std::string(signon_realm),
          .username = base::ASCIIToUTF16(username),
          .compromise_type = type};
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
}

class CompromisedCredentialsProviderTest : public ::testing::Test {
 protected:
  CompromisedCredentialsProviderTest() { store_->Init(/*prefs=*/nullptr); }

  ~CompromisedCredentialsProviderTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  CompromisedCredentialsProvider& provider() { return provider_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
  CompromisedCredentialsProvider provider_{store_, &presenter_};
};

}  // namespace

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsProviderTest,
       NotifyObserversAboutCompromisedCredentialChanges) {
  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1)};

  StrictMockCompromisedCredentialsProviderObserver observer;
  provider().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));

  // Adding the exact same credential should not result in a notification, as
  // the database is not actually modified.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged(IsEmpty()));
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());

  // Similarly to repeated add, a repeated remove should not notify either.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));
}

// Tests removing a compromised credentials by compromise type triggers an
// observer works as expected.
TEST_F(CompromisedCredentialsProviderTest,
       NotifyObserversAboutRemovingCompromisedCredentialsByCompromisedType) {
  CompromisedCredentials phished_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);
  CompromisedCredentials leaked_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);

  StrictMockCompromisedCredentialsProviderObserver observer;
  provider().AddObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(phished_credentials);
  RunUntilIdle();
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(leaked_credentials);
  RunUntilIdle();

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      phished_credentials.signon_realm, phished_credentials.username,
      CompromiseType::kPhished, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(),
              ElementsAre(leaked_credentials));

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      leaked_credentials.signon_realm, leaked_credentials.username,
      CompromiseType::kLeaked, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
  provider().RemoveObserver(&observer);
}

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsProviderTest,
       NotifyObserversAboutSavedPasswordsChanges) {
  StrictMockCompromisedCredentialsProviderObserver observer;
  provider().AddObserver(&observer);

  PasswordForm saved_password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  // Adding a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddLogin(saved_password);
  RunUntilIdle();

  // Updating a saved password should notify observers.
  saved_password.password_value = base::ASCIIToUTF16(kPassword2);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().UpdateLogin(saved_password);
  RunUntilIdle();

  // Removing a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().RemoveLogin(saved_password);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddLogin(saved_password);
  RunUntilIdle();
}

// Tests that the provider is able to join a single password with a compromised
// credential.
TEST_F(CompromisedCredentialsProviderTest, JoinSingleCredentials) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected(credential);
  expected.password = password.password_value;
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that the provider is able to join a password with a credential that was
// compromised in multiple ways.
TEST_F(CompromisedCredentialsProviderTest, JoinPhishedAndLeaked) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials leaked =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);
  CompromisedCredentials phished =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);

  store().AddLogin(password);
  store().AddCompromisedCredentials(leaked);
  store().AddCompromisedCredentials(phished);
  RunUntilIdle();

  CredentialWithPassword expected1(leaked);
  expected1.password = password.password_value;
  CredentialWithPassword expected2(phished);
  expected2.password = password.password_value;
  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that the provider reacts whenever the saved passwords or the
// compromised credentials change.
TEST_F(CompromisedCredentialsProviderTest, ReactToChangesInBothTables) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  std::vector<CredentialWithPassword> expected(credentials.begin(),
                                               credentials.end());
  expected[0].password = passwords[0].password_value;
  expected[1].password = passwords[1].password_value;

  store().AddLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());

  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAreArray(expected));

  store().RemoveLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[1]));

  store().RemoveLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that the provider is able to join multiple passwords with compromised
// credentials.
TEST_F(CompromisedCredentialsProviderTest, JoinMultipleCredentials) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credentials[0]);
  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();

  CredentialWithPassword expected1(credentials[0]);
  expected1.password = passwords[0].password_value;

  CredentialWithPassword expected2(credentials[1]);
  expected2.password = passwords[1].password_value;

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with saved passwords with a
// different username results in an empty list.
TEST_F(CompromisedCredentialsProviderTest, JoinWitDifferentUsername) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername2, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with saved passwords with a
// matching username but different signon_realm results in an empty list.
TEST_F(CompromisedCredentialsProviderTest, JoinWitDifferentSignonRealm) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in multiple entries
// when the passwords are distinct.
TEST_F(CompromisedCredentialsProviderTest, JoinWithMultipleDistinctPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword2, "element_2")};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected1(credential);
  expected1.password = passwords[0].password_value;

  CredentialWithPassword expected2(credential);
  expected2.password = passwords[1].password_value;

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in a single entry
// when the passwords are the same.
TEST_F(CompromisedCredentialsProviderTest, JoinWithMultipleRepeatedPasswords) {
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected(credential);
  expected.password = passwords[0].password_value;

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

}  // namespace password_manager
