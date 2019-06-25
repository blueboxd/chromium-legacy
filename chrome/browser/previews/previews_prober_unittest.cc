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

class PreviewsProberTest : public testing::Test {
 public:
  PreviewsProberTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  std::unique_ptr<PreviewsProber> NewProber() {
    return std::make_unique<PreviewsProber>(test_shared_loader_factory_, kName,
                                            kTestUrl,
                                            PreviewsProber::HttpMethod::kGet);
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  void SetResponse(net::HttpStatusCode http_status, net::Error net_error) {
    network::ResourceResponseHead head =
        network::CreateResourceResponseHead(http_status);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(kTestUrl, head, "content", status);
  }

  void VerifyRequest() {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(request->request.url, kTestUrl);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags, net::LOAD_DISABLE_CACHE);
    EXPECT_FALSE(request->request.allow_credentials);
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

  SetResponse(net::HTTP_OK, net::OK);
  RunUntilIdle();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
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

  SetResponse(net::HTTP_OK, net::ERR_FAILED);
  RunUntilIdle();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
}

TEST_F(PreviewsProberTest, HttpError) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive();
  VerifyRequest();

  SetResponse(net::HTTP_NOT_FOUND, net::OK);
  RunUntilIdle();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
}
