// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using autofill::PasswordForm;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;
using password_manager::PasswordStoreDefault;
using password_manager::LoginDatabase;

class CredentialProviderServiceTest : public PlatformTest {
 public:
  CredentialProviderServiceTest() {}

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    password_store_ = CreatePasswordStore();
    password_store_->Init(nullptr);

    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    EXPECT_FALSE([shared_defaults
        boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted]);
    credential_store_ = [[ArchivableCredentialStore alloc] initWithFileURL:nil];
    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        password_store_, credential_store_);
  }

  void TearDown() override {
    credential_provider_service_->Shutdown();
    password_store_->ShutdownOnUIThread();
    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    [shared_defaults removeObjectForKey:
                         kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    PlatformTest::TearDown();
  }

  scoped_refptr<PasswordStoreDefault> CreatePasswordStore() {
    return base::MakeRefCounted<PasswordStoreDefault>(
        std::make_unique<LoginDatabase>(
            test_login_db_file_path(),
            password_manager::IsAccountStore(false)));
  }

 protected:
  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
  scoped_refptr<PasswordStoreDefault> password_store_;
  ArchivableCredentialStore* credential_store_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderServiceTest);
};

// Test that CredentialProviderService can be created.
TEST_F(CredentialProviderServiceTest, Create) {
  EXPECT_TRUE(credential_provider_service_);
}

// Test that CredentialProviderService syncs all the credentials the first time
// it runs.
TEST_F(CredentialProviderServiceTest, FirstSync) {
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return [app_group::GetGroupUserDefaults()
        boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  }));
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, PasswordChanges) {
  EXPECT_EQ(0u, credential_store_.credentials.count);

  PasswordForm form;
  form.origin = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");

  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  ASSERT_EQ(1u, credential_store_.credentials.count);

  NSString* keychainIdentifier =
      credential_store_.credentials.firstObject.keychainIdentifier;
  form.password_value = base::ASCIIToUTF16("secret");

  password_store_->UpdateLogin(form);
  task_environment_.RunUntilIdle();

  // Expect that the credential in the store now has a different keychain
  // identifier.
  id<Credential> credential = credential_store_.credentials.firstObject;
  EXPECT_NSNE(keychainIdentifier, credential.keychainIdentifier);
  ASSERT_EQ(1u, credential_store_.credentials.count);

  password_store_->RemoveLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be empty.
  ASSERT_EQ(0u, credential_store_.credentials.count);
}

}  // namespace
