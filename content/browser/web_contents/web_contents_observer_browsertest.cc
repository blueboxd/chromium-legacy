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

namespace {

class CookieTracker : public WebContentsObserver {
 public:
  explicit CookieTracker(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  struct CookieAccessDescription {
    CookieAccessDetails::Type type;

    GURL url;
    GURL first_party_url;
    std::string cookie_name;
    std::string cookie_value;

    friend std::ostream& operator<<(std::ostream& o,
                                    const CookieAccessDescription& d) {
      o << (d.type == CookieAccessDetails::Type::kRead ? "read" : "change");
      o << " url=" << d.url;
      o << " first_party_url=" << d.first_party_url;
      o << " name=" << d.cookie_name;
      o << " value=" << d.cookie_value;
      return o;
    }

   private:
    auto comparison_key() const {
      return std::tie(type, url, first_party_url, cookie_name, cookie_value);
    }

   public:
    bool operator==(const CookieAccessDescription& other) const {
      return comparison_key() == other.comparison_key();
    }
  };

  void OnCookiesAccessed(const CookieAccessDetails& details) override {
    for (const auto& cookie : details.cookie_list) {
      cookie_accesses_.push_back({details.type, details.url,
                                  details.first_party_url, cookie.Name(),
                                  cookie.Value()});
    }

    QuitIfReady();
  }

  void WaitForCookies(size_t count) {
    waiting_for_cookies_count_ = count;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    QuitIfReady();
    run_loop.Run();
  }

  std::vector<CookieAccessDescription>& cookie_accesses() {
    return cookie_accesses_;
  }

 private:
  void QuitIfReady() {
    if (quit_closure_.is_null())
      return;
    if (cookie_accesses_.size() < waiting_for_cookies_count_)
      return;
    std::move(quit_closure_).Run();
  }

  std::vector<CookieAccessDescription> cookie_accesses_;

  size_t waiting_for_cookies_count_ = 0;
  base::OnceClosure quit_closure_;
};

using CookieAccess = CookieTracker::CookieAccessDescription;

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_MainFrame) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(
      embedded_test_server()->GetURL("a.com", "/cookies/set_cookie.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to |url1|. This navigation should set a cookie, which we should
  // be notified about.
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));
  cookie_tracker.WaitForCookies(1);

  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kChange,
                                        url1, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Navigate to |url2| on the same site. Given that we have set a cookie
  // before, this should sent a previously set cookie with the request and we
  // should be notified about this.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));
  cookie_tracker.WaitForCookies(1);

  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead, url2,
                                        first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_MainFrameRedirect) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/redirect_and_set_cookie.html"));
  GURL url1_after_redirect(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to |url1|. The initial URL redirects and sets a cookie (we
  // should be notified about this) and as the redirect points to the same site,
  // cookie should be sent for the second request as well (we should be notified
  // about this as well).
  EXPECT_TRUE(NavigateToURL(web_contents(), url1, url1_after_redirect));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::UnorderedElementsAre(
          CookieAccess{CookieAccessDetails::Type::kChange, url1,
                       first_party_url, "foo", "bar"},
          CookieAccess{CookieAccessDetails::Type::kRead, url1_after_redirect,
                       first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Navigate to another url on the same site and expect a notification about
  // a read cookie.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead, url2,
                                        first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_Subframe) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/set_cookie_from_subframe.html"));
  GURL url1_subframe(
      embedded_test_server()->GetURL("a.com", "/cookies/set_cookie.html"));
  GURL url2(embedded_test_server()->GetURL("a.com",
                                           "/cookies/page_with_subframe.html"));
  GURL url2_subframe(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Load a page with a subframe. The main resource of the the subframe
  // triggers setting a cookie. We should get a cookie change for the
  // subresource and no cookie read for the main resource.
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  cookie_tracker.WaitForCookies(1);
  // Navigations are: main frame (0), subframe (1).
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kChange,
                               url1_subframe, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  EXPECT_TRUE(NavigateToURL(web_contents(), url2));

  // 2) Load a page with a subframe. Both main frame and subframe should get a
  // cookie read.
  cookie_tracker.WaitForCookies(2);
  // Navigations are: main frame (2), subframe (3).
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kRead, url2,
                               first_party_url, "foo", "bar"},
                  CookieAccess{CookieAccessDetails::Type::kRead, url2_subframe,
                               first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_Subresource) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/set_cookie_from_subresource.html"));
  GURL url1_image(embedded_test_server()->GetURL(
      "a.com", "/cookies/image_with_set_cookie.jpg"));
  GURL url2(embedded_test_server()->GetURL(
      "a.com", "/cookies/page_with_subresource.html"));
  GURL url2_image(embedded_test_server()->GetURL(
      "a.com", "/cookies/image_without_set_cookie.jpg"));

  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // 1) Load a page with a subresource (image), which sets a cookie when
  // fetched.
  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kChange, url1_image,
                               first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Load a page with subresource. Both the page and the resource should get
  // a cookie.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));

  cookie_tracker.WaitForCookies(2);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kRead, url2,
                               first_party_url, "foo", "bar"},
                  CookieAccess{CookieAccessDetails::Type::kRead, url2_image,
                               first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_DocumentCookie) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(web_contents(), url1));
  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie='foo=bar'"));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kChange,
                                        url1, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie"));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead, url1,
                                        first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

}  // namespace content
