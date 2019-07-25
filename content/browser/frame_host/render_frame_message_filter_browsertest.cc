// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-test-utils.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

void SetCookieFromJS(RenderFrameHost* frame, std::string cookie) {
  EvalJsResult result = EvalJs(frame, "document.cookie = '" + cookie + "'");
  EXPECT_TRUE(result.error.empty()) << result.error;
}

std::string GetCookieFromJS(RenderFrameHost* frame) {
  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame, "window.domAutomationController.send(document.cookie);", &cookie));
  return cookie;
}

void SetCookieDirect(WebContentsImpl* tab,
                     const GURL& url,
                     const std::string& cookie_line) {
  auto cookie_obj = net::CanonicalCookie::Create(
      url, cookie_line, base::Time::Now(), net::CookieOptions());

  base::RunLoop run_loop;
  BrowserContext::GetDefaultStoragePartition(tab->GetBrowserContext())
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(
          *cookie_obj, url.scheme(), net::CookieOptions(),
          base::BindLambdaForTesting(
              [&](net::CanonicalCookie::CookieInclusionStatus status) {
                run_loop.Quit();
              }));
  run_loop.Run();
}

std::string GetCookiesDirect(WebContentsImpl* tab, const GURL& url) {
  net::CookieList result;
  base::RunLoop run_loop;
  BrowserContext::GetDefaultStoragePartition(tab->GetBrowserContext())
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url, net::CookieOptions(),
                      base::BindLambdaForTesting(
                          [&](const net::CookieList& cookie_list,
                              const net::CookieStatusList& excluded_cookies) {
                            result = cookie_list;
                            run_loop.Quit();
                          }));
  run_loop.Run();
  return net::CanonicalCookie::BuildCookieLine(result);
}

}  // namespace

class RenderFrameMessageFilterBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Exercises basic cookie operations via javascript, including an http page
// interacting with secure cookies.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, Cookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  // The server sends a HttpOnly cookie. The RenderFrameMessageFilter should
  // never allow this to be sent to any renderer process.
  GURL https_url = https_server.GetURL("/set-cookie?notforjs=1;HttpOnly");
  GURL http_url = embedded_test_server()->GetURL("/frame_with_load_event.html");

  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell(), http_url);
  NavigateToURL(shell2, https_url);

  WebContentsImpl* web_contents_https =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  if (AreDefaultSiteInstancesEnabled()) {
    // Note: Both use the default SiteInstance because the URLs don't require
    // a dedicated process, but these default SiteInstances are not the same
    // object because they come from different BrowsingInstances.
    EXPECT_TRUE(web_contents_http->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_TRUE(web_contents_https->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_NE(web_contents_http->GetSiteInstance(),
              web_contents_https->GetSiteInstance());
    EXPECT_FALSE(web_contents_http->GetSiteInstance()->IsRelatedSiteInstance(
        web_contents_https->GetSiteInstance()));
  } else {
    EXPECT_EQ("http://127.0.0.1/",
              web_contents_http->GetSiteInstance()->GetSiteURL().spec());
    EXPECT_EQ("https://127.0.0.1/",
              web_contents_https->GetSiteInstance()->GetSiteURL().spec());
  }

  EXPECT_NE(web_contents_http->GetSiteInstance()->GetProcess(),
            web_contents_https->GetSiteInstance()->GetProcess());

  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // Non-TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'A=1; secure;';"));
  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'B=2';"));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'C=3;secure;';"));
  EXPECT_EQ("B=2; C=3",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'D=4';"));
  EXPECT_EQ("B=2; C=3; D=4",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2; D=4", GetCookieFromJS(web_contents_http->GetMainFrame()));
}

// Ensure "priority" cookie option is settable via document.cookie.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, CookiePriority) {
  ASSERT_TRUE(embedded_test_server()->Start());

  struct {
    std::string param;
    net::CookiePriority priority;
  } cases[] = {{"name=value", net::COOKIE_PRIORITY_DEFAULT},
               {"name=value;priority=Low", net::COOKIE_PRIORITY_LOW},
               {"name=value;priority=Medium", net::COOKIE_PRIORITY_MEDIUM},
               {"name=value;priority=High", net::COOKIE_PRIORITY_HIGH}};

  for (auto test_case : cases) {
    GURL url = embedded_test_server()->GetURL("/set_document_cookie.html?" +
                                              test_case.param);
    NavigateToURL(shell(), url);
    std::vector<net::CanonicalCookie> cookies =
        GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(), url);

    EXPECT_EQ(1u, cookies.size());
    EXPECT_EQ("name", cookies[0].Name());
    EXPECT_EQ("value", cookies[0].Value());
    EXPECT_EQ(test_case.priority, cookies[0].Priority());
  }
}

// SameSite cookies (that aren't marked as http-only) should be available to
// JavaScript.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, SameSiteCookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // The server sets five cookies on 'a.com' and on 'b.com', then loads a
  // page that frames both 'a.com' and 'b.com' under 'a.com'.
  std::string cookies_to_set =
      "/set-cookie?normal=1"
      "&strict=1;SameSite=Strict"
      "&lax=1;SameSite=Lax"
      "&strict-http=1;SameSite=Strict;httponly"
      "&lax-http=1;SameSite=Lax;httponly";

  GURL url = embedded_test_server()->GetURL("a.com", cookies_to_set);
  NavigateToURL(shell(), url);
  url = embedded_test_server()->GetURL("b.com", cookies_to_set);
  NavigateToURL(shell(), url);
  url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(),b())");
  NavigateToURL(shell(), url);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  RenderFrameHost* a_iframe =
      web_contents->GetFrameTree()->root()->child_at(0)->current_frame_host();
  RenderFrameHost* b_iframe =
      web_contents->GetFrameTree()->root()->child_at(1)->current_frame_host();

  // The top-level frame should get both kinds of same-site cookies.
  EXPECT_EQ("normal=1; strict=1; lax=1", GetCookieFromJS(main_frame));

  // Same-site cookies will be delievered to the 'a.com' frame, as it is same-
  // site with its ancestors.
  EXPECT_EQ("normal=1; strict=1; lax=1", GetCookieFromJS(a_iframe));

  // Same-site cookies should not be delievered to the 'b.com' frame, as it
  // isn't same-site with its ancestors.
  EXPECT_EQ("normal=1", GetCookieFromJS(b_iframe));
}

class RestrictedCookieManagerInterceptor
    : public network::mojom::RestrictedCookieManagerInterceptorForTesting {
 public:
  RestrictedCookieManagerInterceptor(
      network::mojom::RestrictedCookieManagerRequest request,
      network::mojom::RestrictedCookieManagerPtr real_rcm)
      : binding_(this, std::move(request)), real_rcm_(std::move(real_rcm)) {}

  void set_override_url(base::Optional<std::string> maybe_url) {
    override_url_ = std::move(maybe_url);
  }

  void SetCookieFromString(const GURL& url,
                           const GURL& site_for_cookies,
                           const std::string& cookie,
                           SetCookieFromStringCallback callback) override {
    GetForwardingInterface()->SetCookieFromString(
        URLToUse(url), std::move(site_for_cookies), std::move(cookie),
        std::move(callback));
  }

  void GetCookiesString(const GURL& url,
                        const GURL& site_for_cookies,
                        GetCookiesStringCallback callback) override {
    GetForwardingInterface()->GetCookiesString(
        URLToUse(url), std::move(site_for_cookies), std::move(callback));
  }

 private:
  network::mojom::RestrictedCookieManager* GetForwardingInterface() override {
    return real_rcm_.get();
  }

  GURL URLToUse(const GURL& url_in) {
    return override_url_ ? GURL(override_url_.value()) : url_in;
  }

  base::Optional<std::string> override_url_;

  mojo::Binding<network::mojom::RestrictedCookieManager> binding_;
  network::mojom::RestrictedCookieManagerPtr real_rcm_;
};

class CookieStoreContentBrowserClient : public ContentBrowserClient {
 public:
  ~CookieStoreContentBrowserClient() override {}

  bool WillCreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_service_worker,
      int process_id,
      int routing_id,
      network::mojom::RestrictedCookieManagerRequest* request) override {
    network::mojom::RestrictedCookieManagerRequest orig_request =
        std::move(*request);

    network::mojom::RestrictedCookieManagerPtr real_rcm;
    *request = mojo::MakeRequest(&real_rcm);

    rcm_interceptor_ = std::make_unique<RestrictedCookieManagerInterceptor>(
        std::move(orig_request), std::move(real_rcm));
    rcm_interceptor_->set_override_url(override_url_);

    return false;  // only made a proxy, still need the actual impl to be made.
  }

  void set_override_url(base::Optional<std::string> maybe_url) {
    override_url_ = maybe_url;
    if (rcm_interceptor_)
      rcm_interceptor_->set_override_url(override_url_);
  }

 private:
  base::Optional<std::string> override_url_;
  std::unique_ptr<RestrictedCookieManagerInterceptor> rcm_interceptor_;
};

// Cookie access in loader is locked to a particular origin, so messages
// for wrong URLs are rejected.
// TODO(https://crbug.com/954603): This should actually result in renderer
// kills.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest,
                       CrossSiteCookieSecurityEnforcement) {
  // The code under test is only active under site isolation.
  if (!AreAllSitesIsolatedForTesting()) {
    return;
  }

  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_with_load_event.html"));

  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());

  // The iframe on the http page should get its own process.
  FrameTreeVisualizer v;
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  RenderFrameHost* main_frame = tab->GetMainFrame();
  RenderFrameHost* iframe =
      tab->GetFrameTree()->root()->child_at(0)->current_frame_host();

  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  SetCookieDirect(tab, GURL("http://127.0.0.1/"), "A_cookie = parent");
  SetCookieDirect(tab, GURL("http://baz.com/"), "B_cookie = child");
  EXPECT_EQ("A_cookie=parent",
            GetCookiesDirect(tab, GURL("http://127.0.0.1/")));

  EXPECT_EQ("B_cookie=child", GetCookiesDirect(tab, GURL("http://baz.com/")));

  // Try to get cross-site cookies from the subframe's process.
  {
    CookieStoreContentBrowserClient browser_client;
    content::ContentBrowserClient* old_browser_client =
        content::SetBrowserClientForTesting(&browser_client);
    browser_client.set_override_url("http://127.0.0.1/");
    EXPECT_EQ("", GetCookieFromJS(iframe));

    content::SetBrowserClientForTesting(old_browser_client);
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  // Now set a cross-site cookie from the main frame's process.
  {
    CookieStoreContentBrowserClient browser_client;
    content::ContentBrowserClient* old_browser_client =
        content::SetBrowserClientForTesting(&browser_client);

    browser_client.set_override_url("https://baz.com/");
    SetCookieFromJS(iframe, "pwn=ed");

    EXPECT_EQ("B_cookie=child", GetCookiesDirect(tab, GURL("http://baz.com/")));

    content::SetBrowserClientForTesting(old_browser_client);
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetFrameTree()->root()));
}

}  // namespace content
