// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#include "base/files/scoped_temp_dir.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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
    credential_provider_service_ =
        std::make_unique<CredentialProviderService>(password_store_, nil);
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

}  // namespace
