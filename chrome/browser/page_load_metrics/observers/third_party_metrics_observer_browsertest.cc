// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead";
const char kWriteCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieWrite";
const char kAccessLocalStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess";
const char kAccessSessionStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess";

class ThirdPartyMetricsObserverBrowserTest : public InProcessBrowserTest {
 protected:
  ThirdPartyMetricsObserverBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~ThirdPartyMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for 127.0.0.1 or localhost, so this
    // is needed to load pages from other hosts (b.com, c.com) without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameToUrl(page);
  }

  void NavigateFrameToUrl(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", url));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // This is needed because third party cookies must be marked SameSite=None and
  // Secure, so they must be accessed over HTTPS.
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserverBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest, NoStorageEvent) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kAccessLocalStorageHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kAccessSessionStorageHistogram, 0, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyCookiesReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Should read a same-origin cookie.
  NavigateFrameTo("a.com", "/set-cookie?same-origin");  // same-origin write
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("b.com", "/");
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyCookiesIPAddress) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  GURL url =
      https_server()->GetURL("/set-cookie?thirdparty=1;SameSite=None;Secure");
  // Hostname is an IP address.
  ASSERT_EQ(
      "",
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  NavigateFrameToUrl(url);           // 3p cookie write
  NavigateFrameTo(url.host(), "/");  // 3p cookie read
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       MultipleThirdPartyCookiesReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("b.com", "/");
  // 3p cookie write
  NavigateFrameTo("c.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("c.com", "/");
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 2, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyDocCookieReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("a.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Write a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';"));

  // Read a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieReadNoWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  // No read is counted since no cookie has previously been set.
  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieWriteNoRead) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

class ThirdPartyDomStorageAccessMetricsObserverBrowserTest
    : public ThirdPartyMetricsObserverBrowserTest,
      public ::testing::WithParamInterface<bool /* is_local_access */> {
 public:
  void InvokeStorageAccessOnFrame(content::RenderFrameHost* frame) const {
    if (GetParam()) {
      EXPECT_TRUE(content::ExecJs(frame, "window.localStorage;"));
    } else {
      EXPECT_TRUE(content::ExecJs(frame, "window.sessionStorage;"));
    }
  }

  const char* DomStorageHistogramName() const {
    return GetParam() ? kAccessLocalStorageHistogram
                      : kAccessSessionStorageHistogram;
  }
};

IN_PROC_BROWSER_TEST_P(ThirdPartyDomStorageAccessMetricsObserverBrowserTest,
                       FirstPartyDomStorageAccess) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("a.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(DomStorageHistogramName(), 0, 1);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyDomStorageAccessMetricsObserverBrowserTest,
                       ThirdPartyDomStorageAccess) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(DomStorageHistogramName(), 1, 1);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyDomStorageAccessMetricsObserverBrowserTest,
                       DuplicateThirdPartyDomStorageAccess) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateFrameTo("c.com", "/empty.html");
  NavigateFrameTo("b.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(DomStorageHistogramName(), 1, 1);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyDomStorageAccessMetricsObserverBrowserTest,
                       MultipleThirdPartyDomStorageAccess) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateFrameTo("c.com", "/empty.html");
  InvokeStorageAccessOnFrame(ChildFrameAt(web_contents()->GetMainFrame(), 0));

  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(DomStorageHistogramName(), 2, 1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ThirdPartyDomStorageAccessMetricsObserverBrowserTest,
    ::testing::Values(false, true));

}  // namespace
