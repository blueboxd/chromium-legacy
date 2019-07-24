// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAccountId[] = "test_user@gmail.com";

class FakeOAuth2AccessTokenManagerDelegate
    : public OAuth2AccessTokenManager::Delegate {
 public:
  FakeOAuth2AccessTokenManagerDelegate(
      network::TestURLLoaderFactory* test_url_loader_factory)
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                test_url_loader_factory)) {}
  ~FakeOAuth2AccessTokenManagerDelegate() override = default;

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override {
    EXPECT_EQ(CoreAccountId(kTestAccountId), account_id);
    return std::make_unique<OAuth2AccessTokenFetcherImpl>(
        consumer, url_loader_factory, "fake_refresh_token");
  }

  bool HasRefreshToken(const CoreAccountId& account_id) const override {
    return CoreAccountId(kTestAccountId) == account_id;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override {
    return shared_factory_;
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
};

class FakeOAuth2AccessTokenManagerConsumer
    : public TestingOAuth2AccessTokenManagerConsumer {
 public:
  FakeOAuth2AccessTokenManagerConsumer() = default;
  ~FakeOAuth2AccessTokenManagerConsumer() override = default;

  // TestingOAuth2AccessTokenManagerConsumer overrides.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    TestingOAuth2AccessTokenManagerConsumer::OnGetTokenSuccess(request,
                                                               token_response);
    if (closure_)
      std::move(closure_).Run();
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    TestingOAuth2AccessTokenManagerConsumer::OnGetTokenFailure(request, error);
    if (closure_)
      std::move(closure_).Run();
  }

  void SetResponseCompletedClosure(base::OnceClosure closure) {
    closure_ = std::move(closure);
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace

class OAuth2AccessTokenManagerTest : public testing::Test {
 public:
  OAuth2AccessTokenManagerTest()
      : delegate_(&test_url_loader_factory_), token_manager_(&delegate_) {}

  void SetUp() override { account_id_ = CoreAccountId(kTestAccountId); }

  void TearDown() override {
    // Makes sure that all the clean up tasks are run. It's required because of
    // cleaning up OAuth2AccessTokenManager::Fetcher on
    // InformWaitingRequestsAndDelete().
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOAuthTokenResponse(const std::string& token,
                                  net::HttpStatusCode status = net::HTTP_OK) {
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), token, status);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  CoreAccountId account_id_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeOAuth2AccessTokenManagerDelegate delegate_;
  OAuth2AccessTokenManager token_manager_;
  FakeOAuth2AccessTokenManagerConsumer consumer_;
};

// Test if StartRequest gets a response properly.
TEST_F(OAuth2AccessTokenManagerTest, StartRequest) {
  base::RunLoop run_loop;
  consumer_.SetResponseCompletedClosure(run_loop.QuitClosure());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_.StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}
