// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

// These origins are not suitable for keying persistent Trust Tokens state:
// - UnsuitableUntrustworthyOrigin is not potentially trustworthy.
// - UnsuitableNonHttpNonHttpsOrigin is neither HTTP nor HTTPS.
const url::Origin& UnsuitableUntrustworthyOrigin() {
  // Default origins are opaque and, as a consequence, not potentially
  // trustworthy.
  static base::NoDestructor<url::Origin> origin{url::Origin()};
  return *origin;
}

const url::Origin& UnsuitableNonHttpNonHttpsOrigin() {
  const char kUnsuitableNonHttpNonHttpsUrl[] = "file:///";
  static base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GURL(kUnsuitableNonHttpNonHttpsUrl)));
  return *origin;
}

// Creates an IsolationInfo for the given top frame origin. (We co-opt
// CreateForInternalRequest for this purpose, but define an alias because we're
// not actually creating internal requests.)
auto CreateIsolationInfo = &net::IsolationInfo::CreateForInternalRequest;

}  // namespace

class TrustTokenRequestHelperFactoryTest : public ::testing::Test {
 public:
  TrustTokenRequestHelperFactoryTest() {
    suitable_request_ = CreateSuitableRequest();
    suitable_params_ = mojom::TrustTokenParams::New();
    suitable_params_->issuer =
        url::Origin::Create(GURL("https://issuer.example"));
  }

 protected:
  const net::URLRequest& suitable_request() const { return *suitable_request_; }
  const mojom::TrustTokenParams& suitable_params() const {
    return *suitable_params_;
  }

  std::unique_ptr<net::URLRequest> CreateSuitableRequest() {
    auto ret = maker_.MakeURLRequest("https://destination.example");
    ret->set_isolation_info(CreateIsolationInfo(
        url::Origin::Create(GURL("https://toplevel.example"))));
    return ret;
  }

  class NoopTrustTokenKeyCommitmentGetter
      : public TrustTokenKeyCommitmentGetter {
   public:
    NoopTrustTokenKeyCommitmentGetter() = default;
    void Get(const url::Origin& origin,
             base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
                 on_done) const override {}
  };

  TrustTokenStatusOrRequestHelper CreateHelperAndWaitForResult(
      const net::URLRequest& request,
      const mojom::TrustTokenParams& params) {
    base::RunLoop run_loop;
    TrustTokenStatusOrRequestHelper obtained_result;
    PendingTrustTokenStore store;

    store.OnStoreReady(TrustTokenStore::CreateInMemory());
    NoopTrustTokenKeyCommitmentGetter getter;

    TrustTokenRequestHelperFactory(&store, &getter,
                                   base::BindRepeating([]() { return true; }))
        .CreateTrustTokenHelperForRequest(
            request, params,
            base::BindLambdaForTesting(
                [&](TrustTokenStatusOrRequestHelper result) {
                  obtained_result = std::move(result);
                  run_loop.Quit();
                }));

    run_loop.Run();
    return obtained_result;
  }

 private:
  base::test::TaskEnvironment env_;
  TestURLRequestMaker maker_;
  std::unique_ptr<net::URLRequest> suitable_request_;
  mojom::TrustTokenParamsPtr suitable_params_;
};

TEST_F(TrustTokenRequestHelperFactoryTest, MissingTopFrameOrigin) {
  std::unique_ptr<net::URLRequest> request = CreateSuitableRequest();
  request->set_isolation_info(net::IsolationInfo());

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_params()).status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

TEST_F(TrustTokenRequestHelperFactoryTest, UnsuitableTopFrameOrigin) {
  auto request = CreateSuitableRequest();
  request->set_isolation_info(
      CreateIsolationInfo(UnsuitableUntrustworthyOrigin()));

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_params()).status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);

  request->set_isolation_info(
      CreateIsolationInfo(UnsuitableNonHttpNonHttpsOrigin()));
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_params()).status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

TEST_F(TrustTokenRequestHelperFactoryTest, ForbiddenHeaders) {
  for (const base::StringPiece& header : TrustTokensRequestHeaders()) {
    std::unique_ptr<net::URLRequest> my_request = CreateSuitableRequest();
    my_request->SetExtraRequestHeaderByName(std::string(header), " ",
                                            /*overwrite=*/true);

    EXPECT_EQ(
        CreateHelperAndWaitForResult(*my_request, suitable_params()).status(),
        mojom::TrustTokenOperationStatus::kInvalidArgument);
  }
}

TEST_F(TrustTokenRequestHelperFactoryTest,
       CreatingSigningHelperRequiresSuitableIssuer) {
  auto request = CreateSuitableRequest();

  auto params = suitable_params().Clone();
  params->type = mojom::TrustTokenOperationType::kSigning;
  params->issuer.reset();

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);

  params->issuer = UnsuitableUntrustworthyOrigin();
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);

  params->issuer = UnsuitableNonHttpNonHttpsOrigin();
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesSigningHelper) {
  auto params = suitable_params().Clone();
  params->type = mojom::TrustTokenOperationType::kSigning;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesIssuanceHelper) {
  auto params = suitable_params().Clone();
  params->type = mojom::TrustTokenOperationType::kIssuance;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesRedemptionHelper) {
  auto params = suitable_params().Clone();
  params->type = mojom::TrustTokenOperationType::kRedemption;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());
}

TEST_F(TrustTokenRequestHelperFactoryTest, RespectsAuthorizer) {
  base::RunLoop run_loop;
  TrustTokenStatusOrRequestHelper obtained_result;
  PendingTrustTokenStore store;

  store.OnStoreReady(TrustTokenStore::CreateInMemory());
  NoopTrustTokenKeyCommitmentGetter getter;

  TrustTokenRequestHelperFactory(&store, &getter,
                                 base::BindRepeating([]() { return false; }))
      .CreateTrustTokenHelperForRequest(
          suitable_request(), suitable_params(),
          base::BindLambdaForTesting(
              [&](TrustTokenStatusOrRequestHelper result) {
                obtained_result = std::move(result);
                run_loop.Quit();
              }));

  run_loop.Run();

  EXPECT_EQ(obtained_result.status(),
            mojom::TrustTokenOperationStatus::kUnavailable);
}
}  // namespace network
