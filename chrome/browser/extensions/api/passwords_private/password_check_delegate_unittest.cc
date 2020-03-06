// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleApp[] = "com.example.app";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "s3cre3t";
constexpr char kPassword2[] = "f00b4r";

using api::passwords_private::CompromisedCredential;
using api::passwords_private::CompromisedCredentialsInfo;
using autofill::PasswordForm;
using password_manager::BulkLeakCheckDelegateInterface;
using password_manager::BulkLeakCheckService;
using password_manager::CompromisedCredentials;
using password_manager::CompromiseType;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::TestPasswordStore;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

PasswordsPrivateEventRouter* CreateAndUsePasswordsPrivateEventRouter(
    Profile* profile) {
  return static_cast<PasswordsPrivateEventRouter*>(
      PasswordsPrivateEventRouterFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating([](content::BrowserContext* context) {
                return std::unique_ptr<KeyedService>(
                    PasswordsPrivateEventRouter::Create(context));
              })));
}

EventRouter* CreateAndUseEventRouter(Profile* profile) {
  // The factory function only requires that T be a KeyedService. Ensure it is
  // actually derived from EventRouter to avoid undefined behavior.
  return static_cast<EventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating([](content::BrowserContext* context) {
            return std::unique_ptr<KeyedService>(
                std::make_unique<EventRouter>(context, nullptr));
          })));
}

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    Profile* profile) {
  return base::WrapRefCounted(static_cast<TestPasswordStore*>(
      PasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get()));
}

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return static_cast<BulkLeakCheckService*>(
      BulkLeakCheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindLambdaForTesting([identity_manager](
                                                  content::BrowserContext*) {
            return std::unique_ptr<
                KeyedService>(std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>()));
          })));
}

CompromisedCredentials MakeCompromised(
    base::StringPiece signon_realm,
    base::StringPiece username,
    base::TimeDelta time_since_creation = base::TimeDelta(),
    CompromiseType compromise_type = CompromiseType::kLeaked) {
  return {
      std::string(signon_realm),
      base::ASCIIToUTF16(username),
      base::Time::Now() - time_since_creation,
      compromise_type,
  };
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password = kPassword1,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
}

std::string MakeAndroidRealm(base::StringPiece package_name) {
  return base::StrCat({"android://hash@", package_name});
}

PasswordForm MakeSavedAndroidPassword(
    base::StringPiece package_name,
    base::StringPiece username,
    base::StringPiece app_display_name = "",
    base::StringPiece affiliated_web_realm = "") {
  PasswordForm form;
  form.signon_realm = MakeAndroidRealm(package_name);
  form.username_value = base::ASCIIToUTF16(username);
  form.app_display_name = std::string(app_display_name);
  form.affiliated_web_realm = std::string(affiliated_web_realm);
  return form;
}

// Creates matcher for a given compromised credential
auto ExpectCompromisedCredential(
    const std::string& formatted_origin,
    const std::string& change_password_url,
    const std::string& username,
    const std::string& elapsed_time_since_compromise,
    api::passwords_private::CompromiseType compromise_type) {
  return AllOf(
      Field(&CompromisedCredential::formatted_origin, formatted_origin),
      Field(&CompromisedCredential::change_password_url,
            Pointee(change_password_url)),
      Field(&CompromisedCredential::username, username),
      Field(&CompromisedCredential::elapsed_time_since_compromise,
            elapsed_time_since_compromise),
      Field(&CompromisedCredential::compromise_type, compromise_type));
}

class PasswordCheckDelegateTest : public ::testing::Test {
 public:
  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  TestEventRouterObserver& event_router_observer() {
    return event_router_observer_;
  }
  TestPasswordStore& store() { return *store_; }
  BulkLeakCheckService* service() { return bulk_leak_check_service_; }
  PasswordCheckDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  EventRouter* event_router_ = CreateAndUseEventRouter(&profile_);
  PasswordsPrivateEventRouter* password_router_ =
      CreateAndUsePasswordsPrivateEventRouter(&profile_);
  TestEventRouterObserver event_router_observer_{event_router_};
  BulkLeakCheckService* bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  PasswordCheckDelegate delegate_{&profile_};
};

}  // namespace

// Sets up the password store with a couple of passwords and compromised
// credentials. Verifies that the result is ordered in such a way that phished
// credentials are before leaked credentials and that within each group
// credentials are ordered by recency.
TEST_F(PasswordCheckDelegateTest, GetCompromisedCredentialsInfoOrders) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2));

  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(1),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername2, base::TimeDelta::FromMinutes(2),
                      CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername2, base::TimeDelta::FromMinutes(3),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername1, base::TimeDelta::FromMinutes(4),
                      CompromiseType::kPhished));
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials,
      ElementsAre(ExpectCompromisedCredential(
                      "example.com", kExampleCom, kUsername2, "2 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kUsername1, "4 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.com", kExampleCom, kUsername1, "1 minute ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED),
                  ExpectCompromisedCredential(
                      "example.org", kExampleOrg, kUsername2, "3 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

TEST_F(PasswordCheckDelegateTest, GetCompromisedCredentialsInfoInjectsAndroid) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddLogin(MakeSavedAndroidPassword(kExampleApp, kUsername2,
                                            "Example App", kExampleCom));
  // Test Android credential without affiliation information.
  store().AddLogin(MakeSavedAndroidPassword(kExampleApp, kUsername1));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(5),
                      CompromiseType::kLeaked));
  store().AddCompromisedCredentials(
      MakeCompromised(MakeAndroidRealm(kExampleApp), kUsername2,
                      base::TimeDelta::FromDays(3), CompromiseType::kPhished));
  store().AddCompromisedCredentials(
      MakeCompromised(MakeAndroidRealm(kExampleApp), kUsername1,
                      base::TimeDelta::FromDays(4), CompromiseType::kPhished));
  RunUntilIdle();

  // Verify that the compromised credentials match what is stored in the
  // password store.
  EXPECT_THAT(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials,
      ElementsAre(ExpectCompromisedCredential(
                      "Example App", kExampleCom, kUsername2, "3 days ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "App (com.example.app)", "", kUsername1, "4 days ago",
                      api::passwords_private::COMPROMISE_TYPE_PHISHED),
                  ExpectCompromisedCredential(
                      "example.com", kExampleCom, kUsername1, "5 minutes ago",
                      api::passwords_private::COMPROMISE_TYPE_LEAKED)));
}

// Test that a change to compromised credential notifies observers.
TEST_F(PasswordCheckDelegateTest, OnGetCompromisedCredentialsInfo) {
  const char* const kEventName =
      api::passwords_private::OnCompromisedCredentialsInfoChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the compromised credential provider
  // is initialized.
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_COMPROMISED_CREDENTIALS_INFO_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to AddCompromisedCredentials results in the
  // expected event.
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_COMPROMISED_CREDENTIALS_INFO_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongId) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential = std::move(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials.at(0));
  EXPECT_EQ(0, credential.id);

  // Purposefully set a wrong id and verify that trying to get a plaintext
  // password fails.
  credential.id = 1;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongSignonRealm) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential = std::move(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials.at(0));
  EXPECT_EQ(kExampleCom, credential.signon_realm);

  // Purposefully set a wrong signon realm and verify that trying to get a
  // plaintext password fails.
  credential.signon_realm = kExampleOrg;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordRejectsWrongUsername) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential = std::move(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials.at(0));
  EXPECT_EQ(kUsername1, credential.username);

  // Purposefully set a wrong username and verify that trying to get a
  // plaintext password fails.
  credential.signon_realm = kUsername2;
  EXPECT_EQ(base::nullopt,
            delegate().GetPlaintextCompromisedPassword(std::move(credential)));
}

TEST_F(PasswordCheckDelegateTest,
       GetPlaintextCompromisedPasswordReturnsCorrectPassword) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  CompromisedCredential credential = std::move(
      delegate().GetCompromisedCredentialsInfo().compromised_credentials.at(0));
  EXPECT_EQ(0, credential.id);
  EXPECT_EQ(kExampleCom, credential.signon_realm);
  EXPECT_EQ(kUsername1, credential.username);
  EXPECT_EQ(nullptr, credential.password);

  base::Optional<CompromisedCredential> opt_credential =
      delegate().GetPlaintextCompromisedPassword(std::move(credential));
  ASSERT_TRUE(opt_credential.has_value());
  EXPECT_EQ(0, opt_credential->id);
  EXPECT_EQ(kExampleCom, opt_credential->signon_realm);
  EXPECT_EQ(kUsername1, opt_credential->username);
  EXPECT_EQ(kPassword1, *opt_credential->password);
}

// Test that changing a compromised password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialIdMismatch) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  CompromisedCredential& credential = info.compromised_credentials.at(0);
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().ChangeCompromisedCredential(credential, "new_pass"));
}

// Test that changing a compromised password fails if the underlying compromised
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialStaleData) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  CompromisedCredential& credential = info.compromised_credentials.at(0);

  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_FALSE(delegate().ChangeCompromisedCredential(credential, "new_pass"));
}

// Test that changing a compromised password succeeds.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialSuccess) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  CompromisedCredential credential =
      std::move(info.compromised_credentials.at(0));
  EXPECT_EQ(0, credential.id);
  EXPECT_EQ(kExampleCom, credential.signon_realm);
  EXPECT_EQ(kUsername1, credential.username);
  EXPECT_EQ(base::UTF8ToUTF16(kPassword1),
            store().stored_passwords().at(kExampleCom).at(0).password_value);

  EXPECT_TRUE(delegate().ChangeCompromisedCredential(credential, kPassword2));
  RunUntilIdle();

  EXPECT_EQ(base::UTF8ToUTF16(kPassword2),
            store().stored_passwords().at(kExampleCom).at(0).password_value);
}

// Test that changing a compromised password removes duplicates from store.
TEST_F(PasswordCheckDelegateTest, ChangeCompromisedCredentialRemovesDupes) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1,
                                     "different_element"));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_EQ(2u, store().stored_passwords().at(kExampleCom).size());
  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();

  CompromisedCredential credential =
      std::move(info.compromised_credentials.at(0));
  EXPECT_TRUE(delegate().ChangeCompromisedCredential(credential, kPassword2));
  RunUntilIdle();

  EXPECT_EQ(1u, store().stored_passwords().at(kExampleCom).size());
  EXPECT_EQ(
      kPassword2,
      base::UTF16ToUTF8(
          store().stored_passwords().at(kExampleCom).at(0).password_value));
}

// Test that removing a compromised password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialIdMismatch) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  CompromisedCredential& credential = info.compromised_credentials.at(0);
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Test that removing a compromised password fails if the underlying compromised
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialStaleData) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  const CompromisedCredential& credential = info.compromised_credentials.at(0);

  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Test that removing a compromised password succeeds.
TEST_F(PasswordCheckDelegateTest, RemoveCompromisedCredentialSuccess) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddCompromisedCredentials(MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  api::passwords_private::CompromisedCredentialsInfo info =
      delegate().GetCompromisedCredentialsInfo();
  const CompromisedCredential& credential = info.compromised_credentials.at(0);
  EXPECT_TRUE(delegate().RemoveCompromisedCredential(credential));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  // Expect another removal of the same credential to fail.
  EXPECT_FALSE(delegate().RemoveCompromisedCredential(credential));
}

// Tests that we don't create an entry in the database if there is no matching
// saved password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundDoesNotCreateCredential) {
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
}

// Test that we don't create an entry in the password store if IsLeaked is
// false.
TEST_F(PasswordCheckDelegateTest, NoLeakedFound) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(false));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
}

// Test that a found leak creates a compromised credential in the password
// store.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesCredential) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  RunUntilIdle();

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(),
              ElementsAre(CompromisedCredentials{
                  .signon_realm = kExampleCom,
                  .username = base::ASCIIToUTF16(kUsername1),
                  .create_time = base::Time::Now(),
                  .compromise_type = CompromiseType::kLeaked,
              }));
}

// Test that a found leak creates a compromised credential in the password
// store for each combination of the same canonicalized username and password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesMultipleCredential) {
  const std::string kUsername2Upper = base::ToUpperASCII(kUsername2);
  const std::string kUsername2Email = base::StrCat({kUsername2, "@email.com"});

  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2Upper, kPassword2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2Email, kPassword2));
  RunUntilIdle();

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername1),
                          base::ASCIIToUTF16(kPassword1)),
      IsLeaked(true));
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername2Email),
                          base::ASCIIToUTF16(kPassword2)),
      IsLeaked(true));
  RunUntilIdle();

  EXPECT_THAT(store().compromised_credentials(),
              UnorderedElementsAre(
                  CompromisedCredentials{
                      .signon_realm = kExampleCom,
                      .username = base::ASCIIToUTF16(kUsername1),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleOrg,
                      .username = base::ASCIIToUTF16(kUsername1),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleCom,
                      .username = base::ASCIIToUTF16(kUsername2Upper),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  },
                  CompromisedCredentials{
                      .signon_realm = kExampleOrg,
                      .username = base::ASCIIToUTF16(kUsername2Email),
                      .create_time = base::Time::Now(),
                      .compromise_type = CompromiseType::kLeaked,
                  }));
}

}  // namespace extensions
