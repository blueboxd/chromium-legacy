// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using password_manager::TestPasswordStore;

class CredentialProviderServiceTest : public testing::Test {
 public:
  CredentialProviderServiceTest() {}

  ~CredentialProviderServiceTest() override {
    if (credential_provider_service_) {
      credential_provider_service_->Shutdown();
      password_store_->ShutdownOnUIThread();
    }
  }

  void CreateConsentService() {
    password_store_ = base::MakeRefCounted<TestPasswordStore>();
    credential_provider_service_ =
        std::make_unique<CredentialProviderService>(password_store_);
  }

 protected:
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
  scoped_refptr<TestPasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderServiceTest);
};

TEST_F(CredentialProviderServiceTest, Create) {
  CreateConsentService();
  EXPECT_TRUE(credential_provider_service_);
}

}  // namespace
