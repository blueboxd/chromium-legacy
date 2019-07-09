// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"

namespace content {

class ServiceWorkerNetworkIsolationKeyBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          bool /* test_same_network_isolation_key */> {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByTopFrameOrigin);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Does a cross-process navigation to clear the in-memory cache.
  // We are relying on this navigation to discard the old process.
  void CrossProcessNavigation() {
    RenderProcessHost* process =
        shell()->web_contents()->GetMainFrame()->GetProcess();
    RenderProcessHostWatcher process_watcher(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    NavigateToURL(shell(), GetWebUIURL("version"));
    process_watcher.Wait();
  }

  // Register a service worker |main_script_file| in the scope of
  // |embedded_test_server|'s origin, that does
  // importScripts(|import_script_url|) and fetch(|fetch_url|).
  void RegisterServiceWorkerThatDoesImportScriptsAndFetch(
      const net::EmbeddedTestServer* embedded_test_server,
      const std::string& main_script_file,
      const GURL& import_script_url,
      const GURL& fetch_url) {
    content::TestNavigationObserver navigation_observer(
        shell()->web_contents(), /*number_of_navigations*/ 1,
        content::MessageLoopRunner::QuitMode::DEFERRED);
    std::string subframe_url =
        embedded_test_server
            ->GetURL("/service_worker/create_service_worker.html")
            .spec();

    std::string subframe_name = GetUniqueSubframeName();
    EvalJsResult result =
        EvalJs(shell()->web_contents()->GetMainFrame(),
               JsReplace("createFrame($1, $2)", subframe_url, subframe_name));
    ASSERT_TRUE(result.error.empty());
    navigation_observer.Wait();

    RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
        shell()->web_contents(),
        base::BindRepeating(&FrameMatchesName, subframe_name));
    DCHECK(subframe_rfh);

    std::string main_script_file_with_param = base::StrCat(
        {main_script_file, "?import_script_url=", import_script_url.spec(),
         "&fetch_url=", fetch_url.spec()});

    EXPECT_EQ("DONE",
              EvalJs(subframe_rfh,
                     JsReplace("register($1)", main_script_file_with_param)));
  }

 private:
  std::string GetUniqueSubframeName() {
    subframe_id_ += 1;
    return "subframe_name_" + base::NumberToString(subframe_id_);
  }

  size_t subframe_id_ = 0;
  base::test::ScopedFeatureList feature_list_;
};

// Test that network isolation key is filled in correctly for service workers.
// It checks the cache status of importScripts() as well as fetch() request from
// two different service workers, where the two workers may be from the same or
// different origin - network isolation key. When the origins are the same, we
// expect the 2nd importScripts and/or fetch request to exist in the cache, and
// when the origins are different, we expect the 2nd request to not exist in the
// cache. The imported/fetched script are always the same as it's a control
// variable for this test.
IN_PROC_BROWSER_TEST_P(ServiceWorkerNetworkIsolationKeyBrowserTest,
                       ImportScriptsAndFetchRequest) {
  bool test_same_network_isolation_key = GetParam();

  // Discard the old process to clear the in-memory cache.
  CrossProcessNavigation();

  net::EmbeddedTestServer cross_origin_server_1;
  cross_origin_server_1.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server_1.Start());

  net::EmbeddedTestServer cross_origin_server_tmp;
  cross_origin_server_tmp.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server_tmp.Start());

  auto& cross_origin_server_2 = test_same_network_isolation_key
                                    ? cross_origin_server_1
                                    : cross_origin_server_tmp;

  net::EmbeddedTestServer resource_request_server;
  resource_request_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(resource_request_server.Start());
  GURL import_script_url =
      resource_request_server.GetURL("/service_worker/empty.js");
  GURL fetch_url =
      resource_request_server.GetURL("/service_worker/empty2.html");

  std::map<GURL, size_t> request_completed_count;

  base::RunLoop cache_status_waiter;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) { return false; }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url == import_script_url || request_url == fetch_url) {
              size_t& num_completed = request_completed_count[request_url];
              num_completed += 1;
              if (num_completed == 1) {
                EXPECT_FALSE(status.exists_in_cache);
              } else if (num_completed == 2) {
                EXPECT_EQ(status.exists_in_cache,
                          test_same_network_isolation_key);
              } else {
                NOTREACHED();
              }
            }
            if (request_completed_count[import_script_url] == 2 &&
                request_completed_count[fetch_url] == 2) {
              cache_status_waiter.Quit();
            }
          }),
      {});

  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL("/service_worker/frame_factory.html"), 1);

  RegisterServiceWorkerThatDoesImportScriptsAndFetch(
      &cross_origin_server_1, "worker_with_import_and_fetch.js",
      import_script_url, fetch_url);
  RegisterServiceWorkerThatDoesImportScriptsAndFetch(
      &cross_origin_server_2, "worker_with_import_and_fetch_2.js",
      import_script_url, fetch_url);

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ServiceWorkerNetworkIsolationKeyBrowserTest,
                         testing::Bool());

}  // namespace content
