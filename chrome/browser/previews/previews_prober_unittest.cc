// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include "base/test/scoped_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kTestUrl("https://test.com");
const char kName[] = "testing";
}  // namespace

class TestPreviewsProber : public PreviewsProber {
 public:
  TestPreviewsProber(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& name,
      const GURL& url,
      const HttpMethod http_method,
      const RetryPolicy& retry_policy,
      const base::TickClock* tick_clock)
      : PreviewsProber(url_loader_factory,
                       name,
                       url,
                       http_method,
                       retry_policy,
                       tick_clock) {}
};

class PreviewsProberTest : public testing::Test {
 public:
  PreviewsProberTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  std::unique_ptr<PreviewsProber> NewProber() {
    return std::make_unique<TestPreviewsProber>(
        test_shared_loader_factory_, kName, kTestUrl,
        PreviewsProber::HttpMethod::kGet, PreviewsProber::RetryPolicy(),
        scoped_task_environment_.GetMockTickClock());
  }

  std::unique_ptr<PreviewsProber> NewProberWithRetryPolicy(
      const PreviewsProber::RetryPolicy& retry_policy) {
    return std::make_unique<TestPreviewsProber>(
        test_shared_loader_factory_, kName, kTestUrl,
        PreviewsProber::HttpMethod::kGet, retry_policy,
        scoped_task_environment_.GetMockTickClock());
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  void FastForward(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
  }

  void MakeResponseAndWait(net::HttpStatusCode http_status,
                           net::Error net_error) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    ASSERT_EQ(request->request.url.host(), kTestUrl.host());
    ASSERT_EQ(request->request.url.scheme(), kTestUrl.scheme());

    network::ResourceResponseHead head =
        network::CreateResourceResponseHead(http_status);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, head, "content",
                                         status);
    RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  void VerifyNoRequests() {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  }

  void VerifyRequest(bool expect_random_guid = false) {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags, net::LOAD_DISABLE_CACHE);
    EXPECT_FALSE(request->request.allow_credentials);
    if (expect_random_guid) {
      EXPECT_NE(request->request.url, kTestUrl);
      EXPECT_TRUE(request->request.url.query().find("guid=") !=
                  std::string::npos);
      EXPECT_EQ(request->request.url.query().length(),
                5U /* len("guid=") */ + 36U /* len(hex guid with hyphens) */);
      // We don't check for the randomness of successive GUIDs on the assumption
      // base::GenerateGUID() is always correct.
    } else {
      EXPECT_EQ(request->request.url, kTestUrl);
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PreviewsProberTest, OK) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, MultipleStart) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // Calling |SendNowIfInactive| many times should result in only one url
  // request, which is verified in |VerifyRequest|.
  prober->SendNowIfInactive();
  prober->SendNowIfInactive();
  prober->SendNowIfInactive();
  VerifyRequest();
}

TEST_F(PreviewsProberTest, NetError) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, HttpError) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, RandomGUID) {
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.use_random_urls = true;
  retry_policy.max_retries = 0;

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest(true /* expect_random_guid */);

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, RetryLinear) {
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 1000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, RetryExponential) {
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kExponential;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 2000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(1999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}
