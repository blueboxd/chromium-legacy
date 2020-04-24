// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NotNull;

namespace content {

class WebContentsObserverBrowserTest : public ContentBrowserTest {
 public:
  WebContentsObserverBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* top_frame_host() {
    return static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame());
  }

  base::test::ScopedFeatureList feature_list_;
};

namespace {

class ServiceWorkerAccessObserver : public WebContentsObserver {
 public:
  ServiceWorkerAccessObserver(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD3(OnServiceWorkerAccessed,
               void(NavigationHandle*, const GURL&, AllowServiceWorkerResult));
  MOCK_METHOD3(OnServiceWorkerAccessed,
               void(RenderFrameHost*, const GURL&, AllowServiceWorkerResult));
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       OnServiceWorkerAccessed) {
  GURL service_worker_scope =
      embedded_test_server()->GetURL("/service_worker/");
  {
    // 1) Navigate to a page and register a ServiceWorker. Expect a notification
    // to be called when the service worker is accessed from a frame.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer,
        OnServiceWorkerAccessed(
            testing::Matcher<RenderFrameHost*>(NotNull()), service_worker_scope,
            AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(), embedded_test_server()->GetURL(
                            "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(top_frame_host(),
                     "register('fetch_event.js', '/service_worker/');"));
    run_loop.Run();
  }

  {
    // 2) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer,
                OnServiceWorkerAccessed(
                    testing::Matcher<NavigationHandle*>(NotNull()),
                    service_worker_scope,
                    AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }
}

namespace {

class ServiceWorkerAccessContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ServiceWorkerAccessContentBrowserClient() = default;

  void SetJavascriptAllowed(bool allowed) { javascript_allowed_ = allowed; }

  void SetCookiesAllowed(bool allowed) { cookies_allowed_ = allowed; }

  AllowServiceWorkerResult AllowServiceWorkerOnUI(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      BrowserContext* context) override {
    return AllowServiceWorkerResult::FromPolicy(!javascript_allowed_,
                                                !cookies_allowed_);
  }

 private:
  bool cookies_allowed_ = true;
  bool javascript_allowed_ = true;
};

}  // namespace

class WebContentsObserverWithSWonUIBrowserTest
    : public WebContentsObserverBrowserTest {
 public:
  WebContentsObserverWithSWonUIBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kServiceWorkerOnUI);
  }
};

IN_PROC_BROWSER_TEST_F(WebContentsObserverWithSWonUIBrowserTest,
                       OnServiceWorkerAccessed_ContentClientBlocked) {
  GURL service_worker_scope =
      embedded_test_server()->GetURL("/service_worker/");
  {
    // 1) Navigate to a page and register a ServiceWorker. Expect a notification
    // to be called when the service worker is accessed from a frame.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer,
        OnServiceWorkerAccessed(
            testing::Matcher<RenderFrameHost*>(NotNull()), service_worker_scope,
            AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(), embedded_test_server()->GetURL(
                            "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(top_frame_host(),
                     "register('fetch_event.js', '/service_worker/');"));
    run_loop.Run();
  }

  // 2) Set content client and disallow javascript.
  ServiceWorkerAccessContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  content_browser_client.SetJavascriptAllowed(false);

  {
    // 2) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation. Javascript should be blocked according to the policy.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnServiceWorkerAccessed(
                              testing::Matcher<NavigationHandle*>(NotNull()),
                              service_worker_scope,
                              AllowServiceWorkerResult::FromPolicy(
                                  /* javascript_blocked=*/true,
                                  /* cookies_blocked=*/false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }

  content_browser_client.SetJavascriptAllowed(true);
  content_browser_client.SetCookiesAllowed(false);

  {
    // 3) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation. Cookies should be blocked according to the policy.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnServiceWorkerAccessed(
                              testing::Matcher<NavigationHandle*>(NotNull()),
                              service_worker_scope,
                              AllowServiceWorkerResult::FromPolicy(
                                  /* javascript_blocked=*/false,
                                  /* cookies_blocked=*/true)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }

  SetBrowserClientForTesting(old_client);
}

}  // namespace content
