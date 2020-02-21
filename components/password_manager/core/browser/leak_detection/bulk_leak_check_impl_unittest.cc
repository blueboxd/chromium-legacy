// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kTestEmail[] = "user@gmail.com";

LeakCheckCredential TestCredential(base::StringPiece username) {
  return LeakCheckCredential(base::ASCIIToUTF16(username),
                             base::ASCIIToUTF16("password123"));
}

class BulkLeakCheckTest : public testing::Test {
 public:
  BulkLeakCheckTest()
      : bulk_check_(
            &delegate_,
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  MockBulkLeakCheckDelegateInterface& delegate() { return delegate_; }
  BulkLeakCheckImpl& bulk_check() { return bulk_check_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  ::testing::StrictMock<MockBulkLeakCheckDelegateInterface> delegate_;
  BulkLeakCheckImpl bulk_check_;
};

TEST_F(BulkLeakCheckTest, Create) {
  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);
  // Destroying |leak_check_| doesn't trigger anything.
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAndDestroyImmediately) {
  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential("user1"));
  credentials.push_back(TestCredential("user2"));
  bulk_check().CheckCredentials(std::move(credentials));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAndDestroyAfterPayload) {
  AccountInfo info = identity_test_env().MakeAccountAvailable(kTestEmail);
  identity_test_env().SetCookieAccounts({{info.email, info.gaia}});
  identity_test_env().SetRefreshTokenForAccount(info.account_id);

  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential("user1"));
  bulk_check().CheckCredentials(std::move(credentials));
  RunUntilIdle();
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessTokenAuthError) {
  AccountInfo info = identity_test_env().MakeAccountAvailable(kTestEmail);
  identity_test_env().SetCookieAccounts({{info.email, info.gaia}});
  identity_test_env().SetRefreshTokenForAccount(info.account_id);

  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kTokenRequestFailure));

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential("user1"));
  bulk_check().CheckCredentials(std::move(credentials));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessTokenNetError) {
  AccountInfo info = identity_test_env().MakeAccountAvailable(kTestEmail);
  identity_test_env().SetCookieAccounts({{info.email, info.gaia}});
  identity_test_env().SetRefreshTokenForAccount(info.account_id);

  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kNetworkError));

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential("user1"));
  bulk_check().CheckCredentials(std::move(credentials));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));
}

}  // namespace
}  // namespace password_manager
