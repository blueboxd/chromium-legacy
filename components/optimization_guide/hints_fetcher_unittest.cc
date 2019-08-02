// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_fetcher.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

constexpr char optimization_guide_service_url[] = "https://hintsserver.com/";

class HintsFetcherTest : public testing::Test {
 public:
  explicit HintsFetcherTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    base::test::ScopedFeatureList scoped_list;
    scoped_list.InitAndEnableFeatureWithParameters(
        features::kOptimizationHintsFetching, {});

    hints_fetcher_ = std::make_unique<HintsFetcher>(
        shared_url_loader_factory_, GURL(optimization_guide_service_url));
  }

  ~HintsFetcherTest() override {}

  void OnHintsFetched(base::Optional<std::unique_ptr<proto::GetHintsResponse>>
                          get_hints_response) {
    if (get_hints_response)
      hints_fetched_ = true;
  }

  bool hints_fetched() { return hints_fetched_; }

  void SetConnectionOffline() {
    network_tracker_ = network::TestNetworkConnectionTracker::GetInstance();
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetConnectionOnline() {
    network_tracker_ = network::TestNetworkConnectionTracker::GetInstance();
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
  }

 protected:
  bool FetchHints(const std::vector<std::string>& hosts) {
    bool status = hints_fetcher_->FetchOptimizationGuideServiceHints(
        hosts, base::BindOnce(&HintsFetcherTest::OnHintsFetched,
                              base::Unretained(this)));
    RunUntilIdle();
    return status;
  }

  // Return a 200 response with provided content to any pending requests.
  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        optimization_guide_service_url, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  void VerifyHasPendingFetchRequests() {
    EXPECT_GE(test_url_loader_factory_.NumPending(), 1);
    std::string key_value;
    for (const auto& pending_request :
         *test_url_loader_factory_.pending_requests()) {
      EXPECT_EQ(pending_request.request.method, "POST");
      EXPECT_TRUE(net::GetValueForKeyInQuery(pending_request.request.url, "key",
                                             &key_value));
    }
  }

 private:
  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  bool hints_fetched_ = false;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<HintsFetcher> hints_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  network::TestNetworkConnectionTracker* network_tracker_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherTest);
};

TEST_F(HintsFetcherTest, FetchOptimizationGuideServiceHints) {
  std::string response_content;
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());
}

// Tests to ensure that multiple hint fetches by the same object cannot be in
// progress simultaneously.
TEST_F(HintsFetcherTest, FetchInProcess) {
  std::string response_content;
  // Fetch back to back without waiting for Fetch to complete,
  // |fetch_in_progress_| should cause early exit.
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  EXPECT_FALSE(FetchHints(std::vector<std::string>()));

  // Once response arrives, check to make sure a new fetch can start.
  SimulateResponse(response_content, net::HTTP_OK);
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
}

// Tests 404 response from request.
TEST_F(HintsFetcherTest, FetchReturned404) {
  std::string response_content;

  EXPECT_TRUE(FetchHints(std::vector<std::string>()));

  // Send a 404 to HintsFetcher.
  SimulateResponse(response_content, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(hints_fetched());
}

TEST_F(HintsFetcherTest, FetchReturnBadResponse) {
  std::string response_content = "not proto";
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_FALSE(hints_fetched());
}

TEST_F(HintsFetcherTest, FetchAttemptWhenNetworkOffline) {
  SetConnectionOffline();
  std::string response_content;
  EXPECT_FALSE(FetchHints(std::vector<std::string>()));
  EXPECT_FALSE(hints_fetched());

  SetConnectionOnline();
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());
}

}  // namespace optimization_guide
