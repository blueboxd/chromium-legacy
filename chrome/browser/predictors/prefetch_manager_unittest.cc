// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/prefetch_manager.h"

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace predictors {

namespace {

class FakePrefetchManagerDelegate
    : public PrefetchManager::Delegate,
      public base::SupportsWeakPtr<FakePrefetchManagerDelegate> {
 public:
  void PrefetchFinished(std::unique_ptr<PrefetchStats> stats) override {
    finished_urls_.insert(stats->url);
    auto iter = done_callbacks_.find(stats->url);
    if (iter == done_callbacks_.end())
      return;
    auto callback = std::move(iter->second);
    done_callbacks_.erase(iter);
    std::move(callback).Run();
  }

  void WaitForPrefetchFinished(const GURL& url) {
    if (finished_urls_.find(url) != finished_urls_.end())
      return;
    base::RunLoop loop;
    DCHECK(done_callbacks_.find(url) == done_callbacks_.end());
    done_callbacks_[url] = loop.QuitClosure();
    loop.Run();
  }

 private:
  base::flat_set<GURL> finished_urls_;
  base::flat_map<GURL, base::OnceClosure> done_callbacks_;
};

// Creates a NetworkIsolationKey for a main frame navigation to URL.
net::NetworkIsolationKey CreateNetworkIsolationKey(const GURL& main_frame_url) {
  url::Origin origin = url::Origin::Create(main_frame_url);
  return net::NetworkIsolationKey(origin, origin);
}

}  // namespace

// A test fixture for the PrefetchManager.
class PrefetchManagerTest : public testing::Test {
 public:
  PrefetchManagerTest();
  ~PrefetchManagerTest() override = default;

  PrefetchManagerTest(const PrefetchManagerTest&) = delete;
  PrefetchManagerTest& operator=(const PrefetchManagerTest&) = delete;

 protected:
  size_t GetQueuedJobsCount() const {
    return prefetch_manager_->queued_jobs_.size();
  }

  base::test::ScopedFeatureList features_;
  // IO_MAINLOOP is needed for the EmbeddedTestServer.
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakePrefetchManagerDelegate> fake_delegate_;
  std::unique_ptr<PrefetchManager> prefetch_manager_;
};

PrefetchManagerTest::PrefetchManagerTest()
    : profile_(std::make_unique<TestingProfile>()),
      fake_delegate_(std::make_unique<FakePrefetchManagerDelegate>()),
      prefetch_manager_(
          std::make_unique<PrefetchManager>(fake_delegate_->AsWeakPtr(),
                                            profile_.get())) {
  features_.InitAndEnableFeature(features::kLoadingPredictorPrefetch);
}

// Tests prefetching a single URL.
TEST_F(PrefetchManagerTest, OneMainFrameUrlOnePrefetch) {
  GURL main_frame_url("https://abc.invalid");
  GURL subresource_url("https://xyz.invalid/script.js");
  PrefetchRequest request(subresource_url,
                          CreateNetworkIsolationKey(main_frame_url));

  base::RunLoop loop;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        network::ResourceRequest& request = params->url_request;
        EXPECT_EQ(request.url, subresource_url);
        EXPECT_TRUE(request.load_flags & net::LOAD_PREFETCH);

        std::string purpose;
        EXPECT_TRUE(request.headers.GetHeader("Purpose", &purpose));
        EXPECT_EQ(purpose, "prefetch");

        loop.Quit();
        return false;
      }));
  prefetch_manager_->Start(main_frame_url, {request});
  loop.Run();

  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
}

// Tests prefetching multiple URLs.
TEST_F(PrefetchManagerTest, OneMainFrameUrlMultiplePrefetch) {
  net::test_server::EmbeddedTestServer test_server;
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;

  GURL main_frame_url("https://abc.invalid");

  // Set up prefetches one more than the inflight limit.

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < PrefetchManager::kMaxInflightJobs + 1; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  for (size_t i = 0; i < responses.size(); i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.emplace_back(url, CreateNetworkIsolationKey(main_frame_url));
  }

  // Start the prefetching.
  prefetch_manager_->Start(main_frame_url, std::move(requests));

  // Wait for requests up to the inflight limit.
  for (size_t i = 0; i < responses.size() - 1; i++)
    responses[i]->WaitForRequest();

  // Verify there is a queued job. Pump the run loop just to give the manager a
  // chance to incorrectly start the queued job and fail the expectation.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetQueuedJobsCount(), 1u);

  // Finish one request.
  responses.front()->Send("hi");
  responses.front()->Done();

  // Wait for the queued job to start.
  responses.back()->WaitForRequest();
  EXPECT_EQ(GetQueuedJobsCount(), 0u);

  // Finish all requests.
  for (size_t i = 1; i < responses.size(); i++) {
    responses[i]->Send("hi");
    responses[i]->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
}

// Tests prefetching multiple URLs for multiple main frames.
TEST_F(PrefetchManagerTest, MultipleMainFrameUrlMultiplePrefetch) {
  net::test_server::EmbeddedTestServer test_server;
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;

  GURL main_frame_url("https://abc.invalid");
  GURL main_frame_url2("https://def.invalid");

  // Set up prefetches one more than the inflight limit.
  size_t count = PrefetchManager::kMaxInflightJobs;

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < count + 1; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  for (size_t i = 0; i < count; i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.emplace_back(url, CreateNetworkIsolationKey(main_frame_url));
  }
  {
    GURL url = test_server.GetURL(paths[count]);
    requests.emplace_back(url, CreateNetworkIsolationKey(main_frame_url2));
  }

  // Start the prefetching.
  prefetch_manager_->Start(main_frame_url,
                           std::vector<PrefetchRequest>(
                               requests.begin(), requests.begin() + count - 1));
  prefetch_manager_->Start(main_frame_url2,
                           std::vector<PrefetchRequest>(
                               requests.begin() + count - 1, requests.end()));

  // Wait for requests up to the inflight limit.
  for (size_t i = 0; i < responses.size() - 1; i++)
    responses[i]->WaitForRequest();

  // Verify there is a queued job. Pump the run loop just to give the manager a
  // chance to incorrectly start the queued job and fail the expectation.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetQueuedJobsCount(), 1u);

  // Finish one request.
  responses.front()->Send("hi");
  responses.front()->Done();

  // Wait for the queued job to start.
  responses.back()->WaitForRequest();
  EXPECT_EQ(GetQueuedJobsCount(), 0u);

  // Finish all requests.
  for (size_t i = 1; i < responses.size(); i++) {
    responses[i]->Send("hi");
    responses[i]->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
  fake_delegate_->WaitForPrefetchFinished(main_frame_url2);
}

}  // namespace predictors
