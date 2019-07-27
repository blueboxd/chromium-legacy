// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"

namespace content {

struct PrefetchBrowserTestParam {
  explicit PrefetchBrowserTestParam(bool signed_exchange_enabled)
      : signed_exchange_enabled(signed_exchange_enabled) {}
  const bool signed_exchange_enabled;
};

class PrefetchBrowserTest
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<PrefetchBrowserTestParam> {
 public:
  PrefetchBrowserTest() {
    cross_origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }
  ~PrefetchBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enable_features;
    std::vector<base::Feature> disabled_features;
    if (GetParam().signed_exchange_enabled) {
      enable_features.push_back(features::kSignedHTTPExchange);
    } else {
      disabled_features.push_back(features::kSignedHTTPExchange);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
    PrefetchBrowserTestBase::SetUp();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, CrossOrigin) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL(target_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, DoublePrefetch) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      target_path, target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|, but it should hit only once.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, NoCacheAndNoStore) {
  const char* prefetch_path = "/prefetch.html";
  const char* nocache_path = "/target1.html";
  const char* nostore_path = "/target2.html";

  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body>"
                                      "<link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      nocache_path, nostore_path)));
  RegisterResponse(nocache_path,
                   ResponseEntry("<head><title>NoCache Target</title></head>",
                                 "text/html", {{"cache-control", "no-cache"}}));
  RegisterResponse(nostore_path,
                   ResponseEntry("<head><title>NoStore Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop nocache_waiter;
  base::RunLoop nostore_waiter;
  auto nocache_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nocache_path, &nocache_waiter);
  auto nostore_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nostore_path, &nostore_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // fetch count for the both targets.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  nocache_waiter.Run();
  nostore_waiter.Run();
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());
  EXPECT_EQ(1, nostore_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the no-cache URL wouldn't hit the network, because
  // no-cache resource is kept available up to kPrefetchReuseMins.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nocache_path),
                            "NoCache Target");
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());

  // Subsequent navigation to the no-store URL hit the network again, because
  // no-store resource is not cached even for prefetch.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nostore_path),
                            "NoStore Target");
  EXPECT_EQ(2, nostore_request_counter->GetRequestCount());

  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(embedded_test_server()->GetURL(preload_path));

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  NavigateToURLAndWaitTitle(target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, CrossOriginWithPreload) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL(target_path);
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s' "
                                      "crossorigin=\"anonymous\"></body>",
                                      cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(cross_origin_server_->GetURL(preload_path));

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, SignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL preload_url_in_sxg =
      embedded_test_server()->GetURL(preload_path_in_sxg);
  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(embedded_test_server()->GetURL(target_path)), "text/html",
      {base::StringPrintf("Link: <%s>;rel=\"preload\";as=\"script\"",
                          preload_url_in_sxg.spec().c_str())},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // Test after this point requires SignedHTTPExchange support
  if (!GetParam().signed_exchange_enabled)
    return;

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       CrossOriginSignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL target_sxg_url = cross_origin_server_->GetURL(target_sxg_path);
  const GURL preload_url_in_sxg =
      cross_origin_server_->GetURL(preload_path_in_sxg);

  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       target_sxg_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(cross_origin_server_->GetURL(target_path)), "text/html",
      {base::StringPrintf("Link: <%s>;rel=\"preload\";as=\"script\"",
                          preload_url_in_sxg.spec().c_str())},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // Test after this point requires SignedHTTPExchange support
  if (!GetParam().signed_exchange_enabled)
    return;
  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());

  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(preload_url_in_sxg);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

INSTANTIATE_TEST_SUITE_P(PrefetchBrowserTest,
                         PrefetchBrowserTest,
                         testing::Values(PrefetchBrowserTestParam(false),
                                         PrefetchBrowserTestParam(true)));

}  // namespace content
