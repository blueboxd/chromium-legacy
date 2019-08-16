// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::BrowserThread;

namespace {

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 protected:
  CookiePolicyBrowserTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
                                                 value);
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

  void ExpectFrameContent(const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* nested =
        ChildFrameAt(web_contents->GetMainFrame(), 0);
    std::string content;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        nested,
        "window.domAutomationController.send(document.body.textContent)",
        &content));
    EXPECT_EQ(expected, content);
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    GURL url(embedded_test_server()->GetURL(host, path));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* nested =
        ChildFrameAt(web_contents->GetMainFrame(), 0);
    content::TestNavigationObserver load_observer(web_contents);
    ASSERT_TRUE(ExecuteScript(
        nested,
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  void ExpectNestedFrameContent(const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* nested =
        ChildFrameAt(web_contents->GetMainFrame(), 0);
    content::RenderFrameHost* double_nested = ChildFrameAt(nested, 0);
    std::string content;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        double_nested,
        "window.domAutomationController.send(document.body.textContent)",
        &content));
    EXPECT_EQ(expected, content);
  }

  void ExpectCookiesOnHost(const std::string& host,
                           const std::string& expected) {
    EXPECT_EQ(expected,
              content::GetCookies(browser()->profile(),
                                  embedded_test_server()->GetURL(host, "/")));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  SetBlockThirdPartyCookies(false);

  GURL url(embedded_test_server()->GetURL("/set-cookie?cookie1"));

  std::string cookie = content::GetCookies(browser()->profile(), url);
  ASSERT_EQ("", cookie);

  ui_test_utils::NavigateToURL(browser(), url);

  cookie = content::GetCookies(browser()->profile(), url);
  EXPECT_EQ("cookie1", cookie);
}

// Visits a page that is a redirect across domain boundary to a page that sets
// a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       AllowFirstPartyCookiesRedirect) {
  SetBlockThirdPartyCookies(true);

  GURL url(embedded_test_server()->GetURL("/server-redirect?"));
  GURL redirected_url(embedded_test_server()->GetURL("/set-cookie?cookie2"));

  // Change the host name from 127.0.0.1 to www.example.com so it triggers
  // third-party cookie blocking if the first party for cookies URL is not
  // changed when we follow a redirect.
  ASSERT_EQ("127.0.0.1", redirected_url.host());
  GURL::Replacements replacements;
  replacements.SetHostStr("www.example.com");
  redirected_url = redirected_url.ReplaceComponents(replacements);

  std::string cookie =
      content::GetCookies(browser()->profile(), redirected_url);
  ASSERT_EQ("", cookie);

  ui_test_utils::NavigateToURL(browser(),
                               GURL(url.spec() + redirected_url.spec()));

  cookie = content::GetCookies(browser()->profile(), redirected_url);
  EXPECT_EQ("cookie2", cookie);
}

// Third-Party Frame Tests
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowSetting) {
  SetBlockThirdPartyCookies(false);

  NavigateToPageWithFrame("a.com");

  ExpectCookiesOnHost("b.com", "");

  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // the cookie is set:
  NavigateFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameBlockSetting) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");

  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // the cookie is not set:
  NavigateFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/set-cookie?thirdparty");
  ExpectCookiesOnHost("b.com", "");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowReading) {
  SetBlockThirdPartyCookies(false);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(),
                     embedded_test_server()->GetURL("b.com", "/"),
                     "thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  NavigateToPageWithFrame("a.com");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("thirdparty");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameBlockReading) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(),
                     embedded_test_server()->GetURL("b.com", "/"),
                     "thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  NavigateToPageWithFrame("a.com");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is not sent:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
}

}  // namespace
