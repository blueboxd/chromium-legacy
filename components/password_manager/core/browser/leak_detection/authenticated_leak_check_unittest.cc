// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password123";
constexpr char kExampleCom[] = "https://example.com";

class MockLeakDetectionRequest : public LeakDetectionRequestInterface {
 public:
  // LeakDetectionRequestInterface:
  MOCK_METHOD5(LookupSingleLeak,
               void(network::mojom::URLLoaderFactory*,
                    const std::string&,
                    std::string,
                    std::string,
                    LookupSingleLeakCallback));
};

struct TestLeakDetectionRequest : public LeakDetectionRequestInterface {
  ~TestLeakDetectionRequest() override = default;
  // LeakDetectionRequestInterface:
  void LookupSingleLeak(network::mojom::URLLoaderFactory* url_loader_factory,
                        const std::string& access_token,
                        std::string username_hash_prefix,
                        std::string encrypted_payload,
                        LookupSingleLeakCallback callback) override {
    encrypted_payload_ = std::move(encrypted_payload);
    callback_ = std::move(callback);
  }

  std::string encrypted_payload_;
  LookupSingleLeakCallback callback_;
};

class MockLeakDetectionRequestFactory : public LeakDetectionRequestFactory {
 public:
  // LeakDetectionRequestFactory:
  MOCK_CONST_METHOD0(CreateNetworkRequest,
                     std::unique_ptr<LeakDetectionRequestInterface>());
};

// Helper struct for making a fake network request.
struct PayloadAndCallback {
  std::string payload;
  LeakDetectionRequestInterface::LookupSingleLeakCallback callback;
};

class AuthenticatedLeakCheckTest : public testing::Test {
 public:
  AuthenticatedLeakCheckTest()
      : leak_check_(
            &delegate_,
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    auto mock_request_factory =
        std::make_unique<StrictMock<MockLeakDetectionRequestFactory>>();
    request_factory_ = mock_request_factory.get();
    leak_check_.set_network_factory(std::move(mock_request_factory));
  }
  ~AuthenticatedLeakCheckTest() override = default;

  base::test::TaskEnvironment& task_env() { return task_env_; }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  MockLeakDetectionDelegateInterface& delegate() { return delegate_; }
  MockLeakDetectionRequestFactory* request_factory() {
    return request_factory_;
  }
  AuthenticatedLeakCheck& leak_check() { return leak_check_; }

  // Brings |leak_check_| to the state right after the network request.
  // The check is done for |kUsername|/|kPassword| credential on |kExampleCom|.
  // Returns |encrypted_payload| and |callback| arguments of LookupSingleLeak().
  PayloadAndCallback ImitateNetworkRequest();

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  StrictMock<MockLeakDetectionDelegateInterface> delegate_;
  MockLeakDetectionRequestFactory* request_factory_;
  AuthenticatedLeakCheck leak_check_;
};

PayloadAndCallback AuthenticatedLeakCheckTest::ImitateNetworkRequest() {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);

  leak_check().Start(GURL(kExampleCom), base::ASCIIToUTF16(kUsername),
                     base::ASCIIToUTF16(kPassword));
  // Crypto stuff is done here.
  task_env().RunUntilIdle();

  static constexpr char access_token[] = "access_token";
  auto network_request = std::make_unique<TestLeakDetectionRequest>();
  TestLeakDetectionRequest* raw_request = network_request.get();
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Return the access token.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token, base::Time::Max());

  return {std::move(raw_request->encrypted_payload_),
          std::move(raw_request->callback_)};
}

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

TEST_F(AuthenticatedLeakCheckTest, GetAccessTokenBeforeEncryption) {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);
  const std::string access_token = "access_token";

  leak_check().Start(GURL(kExampleCom), base::ASCIIToUTF16(kUsername),
                     base::ASCIIToUTF16(kPassword));
  // Return the access token before the crypto stuff is done.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token, base::Time::Max());

  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(*network_request,
              LookupSingleLeak(_, access_token, ElementsAre(-67, 116, -87),
                               testing::Ne(""), _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));
  // Crypto stuff is done here.
  task_env().RunUntilIdle();
}

TEST_F(AuthenticatedLeakCheckTest, GetAccessTokenAfterEncryption) {
  AccountInfo info = identity_env().MakeAccountAvailable(kTestEmail);
  identity_env().SetCookieAccounts({{info.email, info.account_id}});
  identity_env().SetRefreshTokenForAccount(info.account_id);

  leak_check().Start(GURL(kExampleCom), base::ASCIIToUTF16(kUsername),
                     base::ASCIIToUTF16(kPassword));
  // crypto stuff is done here.
  task_env().RunUntilIdle();

  const std::string access_token = "access_token";
  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(*network_request,
              LookupSingleLeak(_, access_token, ElementsAre(-67, 116, -87),
                               testing::Ne(""), _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Return the access token.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token, base::Time::Max());
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
}

// Perform the whole cycle of a leak check. The server returns data signalling
// that the password wasn't leaked.
TEST_F(AuthenticatedLeakCheckTest, ParseResponse_NoLeak) {
  PayloadAndCallback payload_and_callback = ImitateNetworkRequest();
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(CipherEncryptWithKey(
          ScryptHashUsernameAndPassword("another_username", kPassword),
          key_server)));

  EXPECT_CALL(delegate(), OnLeakDetectionDone(false, GURL(kExampleCom),
                                              base::ASCIIToUTF16(kUsername),
                                              base::ASCIIToUTF16(kPassword)));
  std::move(payload_and_callback.callback).Run(std::move(response));
  task_env().RunUntilIdle();
}

// Perform the whole cycle of a leak check. The server returns data signalling
// that the password was leaked.
TEST_F(AuthenticatedLeakCheckTest, ParseResponse_Leak) {
  PayloadAndCallback payload_and_callback = ImitateNetworkRequest();
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(CipherEncryptWithKey(
          ScryptHashUsernameAndPassword(kUsername, kPassword), key_server)));

  EXPECT_CALL(delegate(), OnLeakDetectionDone(true, GURL(kExampleCom),
                                              base::ASCIIToUTF16(kUsername),
                                              base::ASCIIToUTF16(kPassword)));
  std::move(payload_and_callback.callback).Run(std::move(response));
  task_env().RunUntilIdle();
}

}  // namespace password_manager
