// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kReadCookieHistogram[] = "PageLoad.Clients.ThirdParty.Origins.Read";
const char kWriteCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.Write";

class ThirdPartyMetricsObserverBrowserTest : public InProcessBrowserTest {
 protected:
  ThirdPartyMetricsObserverBrowserTest() = default;
  ~ThirdPartyMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(embedded_test_server()->GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = embedded_test_server()->GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserverBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       NoCookiesReadOrWritten) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
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
  NavigateFrameTo("b.com", "/set-cookie?thirdparty");  // 3p cookie write
  NavigateFrameTo("b.com", "/");                       // 3p cookie read
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       MultipleThirdPartyCookiesReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/set-cookie?thirdparty");  // 3p cookie write
  NavigateFrameTo("b.com", "/");                       // 3p cookie read
  NavigateFrameTo("c.com", "/set-cookie?thirdparty");  // 3p cookie write
  NavigateFrameTo("c.com", "/");                       // 3p cookie read
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 2, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyDocCookieReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("a.com", "/empty.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';"));

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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

}  // namespace
