// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::StrictMock;

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password123";
constexpr char kExampleCom[] = "https://example.com";

class AuthenticatedLeakCheckTest : public testing::Test {
 public:
  AuthenticatedLeakCheckTest()
      : leak_check_(
            &delegate_,
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}
  ~AuthenticatedLeakCheckTest() override = default;

  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  MockLeakDetectionDelegateInterface& delegate() { return delegate_; }
  AuthenticatedLeakCheck& leak_check() { return leak_check_; }

 private:
  base::test::ScopedTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  StrictMock<MockLeakDetectionDelegateInterface> delegate_;
  AuthenticatedLeakCheck leak_check_;
};

}  // namespace

TEST_F(AuthenticatedLeakCheckTest, Create) {
  EXPECT_CALL(delegate(), OnLeakDetectionDone).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);
  // Destroying |leak_check_| doesn't trigger anything.
}

TEST_F(AuthenticatedLeakCheckTest, HasAccountForRequest_SignedIn) {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);
  EXPECT_TRUE(AuthenticatedLeakCheck::HasAccountForRequest(
      identity_env().identity_manager()));
}

TEST_F(AuthenticatedLeakCheckTest, HasAccountForRequest_Syncing) {
  identity_env().SetPrimaryAccount(kTestEmail);
  EXPECT_TRUE(AuthenticatedLeakCheck::HasAccountForRequest(
      identity_env().identity_manager()));
}

TEST_F(AuthenticatedLeakCheckTest, GetAccessToken) {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);

  leak_check().Start(GURL(kExampleCom), base::ASCIIToUTF16(kUsername),
                     base::ASCIIToUTF16(kPassword));
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  EXPECT_EQ("access_token", leak_check().access_token());
}

TEST_F(AuthenticatedLeakCheckTest, GetAccessTokenFailure) {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);

  leak_check().Start(GURL(kExampleCom), base::ASCIIToUTF16(kUsername),
                     base::ASCIIToUTF16(kPassword));

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kTokenRequestFailure));
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ("", leak_check().access_token());
}

}  // namespace password_manager
