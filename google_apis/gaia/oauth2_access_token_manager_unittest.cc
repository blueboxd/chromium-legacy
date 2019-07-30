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
    EXPECT_NE(account_ids_to_refresh_tokens_.find(account_id),
              account_ids_to_refresh_tokens_.end());
    return std::make_unique<OAuth2AccessTokenFetcherImpl>(
        consumer, url_loader_factory,
        account_ids_to_refresh_tokens_[account_id]);
  }

  bool HasRefreshToken(const CoreAccountId& account_id) const override {
    return account_ids_to_refresh_tokens_.find(account_id) !=
           account_ids_to_refresh_tokens_.end();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override {
    return shared_factory_;
  }

  bool HandleAccessTokenFetch(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (access_token_fetch_closure_) {
      std::move(access_token_fetch_closure_).Run();
      return true;
    }
    return false;
  }

  void OnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override {
    if (!on_access_token_invalidated_callback_)
      return;

    EXPECT_EQ(access_token_invalidated_account_id_, account_id);
    EXPECT_EQ(access_token_invalidated_client_id_, client_id);
    EXPECT_EQ(access_token_invalidated_scopes_, scopes);
    EXPECT_EQ(access_token_invalidated_access_token_, access_token);
    std::move(on_access_token_invalidated_callback_).Run();
  }

  void AddAccount(CoreAccountId id, std::string refresh_token) {
    account_ids_to_refresh_tokens_[id] = refresh_token;
  }

  void SetAccessTokenHandleClosure(base::OnceClosure closure) {
    access_token_fetch_closure_ = std::move(closure);
  }

  void SetOnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token,
      base::OnceClosure callback) {
    access_token_invalidated_account_id_ = account_id;
    access_token_invalidated_client_id_ = client_id;
    access_token_invalidated_scopes_ = scopes;
    access_token_invalidated_access_token_ = access_token;
    on_access_token_invalidated_callback_ = std::move(callback);
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::map<CoreAccountId, std::string> account_ids_to_refresh_tokens_;
  base::OnceClosure access_token_fetch_closure_;
  CoreAccountId access_token_invalidated_account_id_;
  std::string access_token_invalidated_client_id_;
  OAuth2AccessTokenManager::ScopeSet access_token_invalidated_scopes_;
  std::string access_token_invalidated_access_token_;
  base::OnceClosure on_access_token_invalidated_callback_;
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

// Any public API surfaces that are wrapped by ProfileOAuth2TokenService are
// unittested as part of the unittests of that class.

class OAuth2AccessTokenManagerTest : public testing::Test {
 public:
  OAuth2AccessTokenManagerTest()
      : delegate_(&test_url_loader_factory_), token_manager_(&delegate_) {}

  void SetUp() override {
    account_id_ = CoreAccountId(kTestAccountId);
    delegate_.AddAccount(account_id_, "fake_refresh_token");
  }

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

// Test that StartRequest gets a response properly.
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

// Test that CancelAllRequests triggers OnGetTokenFailure.
TEST_F(OAuth2AccessTokenManagerTest, CancelAllRequests) {
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_.StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  const CoreAccountId account_id_2("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_.StartRequest(
          account_id_2, OAuth2AccessTokenManager::ScopeSet(), &consumer_));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  token_manager_.CancelAllRequests();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);
}

// Test that CancelRequestsForAccount cancels requests for the specific account.
TEST_F(OAuth2AccessTokenManagerTest, CancelRequestsForAccount) {
  OAuth2AccessTokenManager::ScopeSet scope_set_1;
  scope_set_1.insert("scope1");
  scope_set_1.insert("scope2");
  OAuth2AccessTokenManager::ScopeSet scope_set_2(scope_set_1.begin(),
                                                 scope_set_1.end());
  scope_set_2.insert("scope3");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      token_manager_.StartRequest(account_id_, scope_set_1, &consumer_));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_.StartRequest(account_id_, scope_set_2, &consumer_));

  const CoreAccountId account_id_2("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      token_manager_.StartRequest(account_id_2, scope_set_1, &consumer_));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  token_manager_.CancelRequestsForAccount(account_id_);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);

  token_manager_.CancelRequestsForAccount(account_id_2);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(3, consumer_.number_of_errors_);
}

// Test that StartRequest fetches a network request after ClearCache.
TEST_F(OAuth2AccessTokenManagerTest, ClearCache) {
  base::RunLoop run_loop1;
  consumer_.SetResponseCompletedClosure(run_loop1.QuitClosure());

  std::set<std::string> scope_list;
  scope_list.insert("scope");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_.StartRequest(account_id_, scope_list, &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop1.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_.token_cache().size());

  token_manager_.ClearCache();

  EXPECT_EQ(0U, token_manager_.token_cache().size());
  base::RunLoop run_loop2;
  consumer_.SetResponseCompletedClosure(run_loop2.QuitClosure());

  SimulateOAuthTokenResponse(GetValidTokenResponse("another token", 3600));
  request = token_manager_.StartRequest(account_id_, scope_list, &consumer_);
  run_loop2.Run();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_.token_cache().size());
}

// Test that ClearCacheForAccount clears caches for the specific account.
TEST_F(OAuth2AccessTokenManagerTest, ClearCacheForAccount) {
  base::RunLoop run_loop1;
  consumer_.SetResponseCompletedClosure(run_loop1.QuitClosure());

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      token_manager_.StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop1.Run();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1U, token_manager_.token_cache().size());

  base::RunLoop run_loop2;
  consumer_.SetResponseCompletedClosure(run_loop2.QuitClosure());
  const CoreAccountId account_id_2("account_id_2");
  delegate_.AddAccount(account_id_2, "refreshToken2");
  // Makes a request for |account_id_2|.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_manager_.StartRequest(
          account_id_2, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  run_loop2.Run();

  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(2U, token_manager_.token_cache().size());

  // Clears caches for |account_id_|.
  token_manager_.ClearCacheForAccount(account_id_);
  EXPECT_EQ(1U, token_manager_.token_cache().size());

  base::RunLoop run_loop3;
  consumer_.SetResponseCompletedClosure(run_loop3.QuitClosure());
  SimulateOAuthTokenResponse(GetValidTokenResponse("another token", 3600));
  // Makes a request for |account_id_| again.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      token_manager_.StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  run_loop3.Run();

  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
  EXPECT_EQ(2U, token_manager_.token_cache().size());

  // Clears caches for |account_id_|.
  token_manager_.ClearCacheForAccount(account_id_);
  EXPECT_EQ(1U, token_manager_.token_cache().size());

  // Clears caches for |account_id_2|.
  token_manager_.ClearCacheForAccount(account_id_2);
  EXPECT_EQ(0U, token_manager_.token_cache().size());
}

// Test that StartRequest checks HandleAccessTokenFetch() from |delegate_|
// before FetchOAuth2Token.
TEST_F(OAuth2AccessTokenManagerTest, HandleAccessTokenFetch) {
  base::RunLoop run_loop;
  delegate_.SetAccessTokenHandleClosure(run_loop.QuitClosure());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      token_manager_.StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  run_loop.Run();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ(0U, token_manager_.GetNumPendingRequestsForTesting(
                    GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                    account_id_, OAuth2AccessTokenManager::ScopeSet()));
}

// Test that InvalidateAccessToken triggers OnAccessTokenInvalidated.
TEST_F(OAuth2AccessTokenManagerTest, OnAccessTokenInvalidated) {
  base::RunLoop run_loop;
  OAuth2AccessTokenManager::ScopeSet scope_set;
  scope_set.insert("scope");
  std::string access_token("access_token");
  delegate_.SetOnAccessTokenInvalidated(
      account_id_, GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      scope_set, access_token, run_loop.QuitClosure());
  token_manager_.InvalidateAccessToken(account_id_, scope_set, access_token);
  run_loop.Run();
}
