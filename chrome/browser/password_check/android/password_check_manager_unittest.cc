// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/password_check/android/password_check_ui_status.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/mock_bulk_leak_check_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using password_manager::BulkLeakCheckService;
using password_manager::CompromisedCredentials;
using password_manager::CompromiseType;
using password_manager::MockBulkLeakCheckService;
using password_manager::PasswordCheckUIStatus;
using password_manager::TestPasswordStore;
using testing::_;
using testing::NiceMock;

namespace {

constexpr char kExampleCom[] = "https://example.com";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "s3cre3t";

class MockPasswordCheckManagerObserver : public PasswordCheckManager::Observer {
 public:
  MOCK_METHOD(void, OnSavedPasswordsFetched, (int), (override));

  MOCK_METHOD(void, OnCompromisedCredentialsChanged, (int), (override));

  MOCK_METHOD(void,
              OnPasswordCheckStatusChanged,
              (password_manager::PasswordCheckUIStatus),
              (override));
};

// TODO(crbug.com/1112804): Extract this into a password manager test utils
// file, since it's used across multiple tests.
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
  return BulkLeakCheckServiceFactory::GetInstance()
      ->SetTestingSubclassFactoryAndUse(
          profile, base::BindLambdaForTesting([=](content::BrowserContext*) {
            return std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
          }));
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password = kPassword1,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
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

}  // namespace

class PasswordCheckManagerTest : public testing::Test {
 public:
  void InitializeManager() {
    manager_ =
        std::make_unique<PasswordCheckManager>(&profile_, &mock_observer_);
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  TestPasswordStore& store() { return *store_; }

 protected:
  NiceMock<MockPasswordCheckManagerObserver> mock_observer_;
  std::unique_ptr<PasswordCheckManager> manager_;

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  BulkLeakCheckService* bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
};

TEST_F(PasswordCheckManagerTest, SendsNoPasswordsMessageIfNoPasswordsAreSaved) {
  EXPECT_CALL(mock_observer_, OnPasswordCheckStatusChanged(
                                  PasswordCheckUIStatus::kErrorNoPasswords));
  InitializeManager();
  RunUntilIdle();
}

TEST_F(PasswordCheckManagerTest, OnSavedPasswordsFetched) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));

  EXPECT_CALL(mock_observer_, OnSavedPasswordsFetched(1));
  InitializeManager();
  RunUntilIdle();

  // Verify that OnSavedPasswordsFetched is not called after the initial fetch
  // even if the saved passwords change.
  EXPECT_CALL(mock_observer_, OnSavedPasswordsFetched(_)).Times(0);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  RunUntilIdle();
}

TEST_F(PasswordCheckManagerTest, OnCompromisedCredentialsChanged) {
  // This is called on multiple events: once for saved passwords retrieval,
  // once for compromised credentials retrieval and once when the saved password
  // is added.
  EXPECT_CALL(mock_observer_, OnCompromisedCredentialsChanged(0)).Times(3);
  InitializeManager();
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_CALL(mock_observer_, OnCompromisedCredentialsChanged(1));
  store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1, base::TimeDelta::FromMinutes(1),
                      CompromiseType::kLeaked));
  RunUntilIdle();
}
