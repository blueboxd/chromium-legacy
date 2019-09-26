// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::BrowserThread;

namespace {

const std::vector<std::string> kStorageTypes{
    "Cookie",    "LocalStorage", "FileSystem",   "SessionStorage",
    "IndexedDb", "WebSql",       "CacheStorage", "ServiceWorker",
};

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 protected:
  CookiePolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
                                                 value);
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  void ExpectFrameContent(const std::string& expected) {
    std::string content;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        GetFrame(),
        "window.domAutomationController.send(document.body.textContent)",
        &content));
    EXPECT_EQ(expected, content);
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    GURL url(https_server_.GetURL(host, path));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver load_observer(web_contents);
    ASSERT_TRUE(ExecuteScript(
        GetFrame(),
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  void ExpectNestedFrameContent(const std::string& expected) {
    std::string content;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        GetNestedFrame(),
        "window.domAutomationController.send(document.body.textContent)",
        &content));
    EXPECT_EQ(expected, content);
  }

  void ExpectCookiesOnHost(const std::string& host,
                           const std::string& expected) {
    EXPECT_EQ(expected, content::GetCookies(browser()->profile(),
                                            https_server_.GetURL(host, "/")));
  }

  void SetStorageForFrame(content::RenderFrameHost* frame) {
    for (const auto& data_type : kStorageTypes) {
      bool data;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          frame, "set" + data_type + "()", &data));
      EXPECT_TRUE(data) << data_type;
    }
  }

  void ExpectStorageForFrame(content::RenderFrameHost* frame, bool expected) {
    for (const auto& data_type : kStorageTypes) {
      bool data;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          frame, "has" + data_type + "();", &data));
      EXPECT_EQ(expected, data) << data_type;
    }
  }

  content::RenderFrameHost* GetFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

 protected:
  net::test_server::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// CookiePolicyBrowserTest with a feature list that enables usage of
// TopLevelOrigin for CookieSettings. This is only required until this
// behavior can be enabled by default. https://crbug.com/988398
class CookiePolicyTopLevelOriginBrowserTest : public CookiePolicyBrowserTest {
 public:
  CookiePolicyTopLevelOriginBrowserTest() {
    enable_cookie_controls_.InitAndEnableFeature(
        content_settings::kImprovedCookieControls);
  }

 private:
  base::test::ScopedFeatureList enable_cookie_controls_;
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  SetBlockThirdPartyCookies(false);

  GURL url(https_server_.GetURL("/set-cookie?cookie1"));

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

  GURL url(https_server_.GetURL("/server-redirect?"));
  GURL redirected_url(https_server_.GetURL("/set-cookie?cookie2"));

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
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
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
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
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

IN_PROC_BROWSER_TEST_F(CookiePolicyTopLevelOriginBrowserTest,
                       ThirdPartyCookiesIFrameExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  // Allow all requests to b.com to have cookies.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server_.GetURL("b.com", "/");
  cookie_settings->SetCookieSetting(url, ContentSetting::CONTENT_SETTING_ALLOW);

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
  // verify that the cookie is sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyTopLevelOriginBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty");
  ExpectCookiesOnHost("b.com", "thirdparty");

  // Allow all requests on the top frame domain a.com to have cookies.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server_.GetURL("a.com", "/");
  cookie_settings->SetThirdPartyCookieSetting(
      url, ContentSetting::CONTENT_SETTING_ALLOW);

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
  // verify that the cookie is sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyTopLevelOriginBrowserTest,
                       ThirdPartyIFrameStorage) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetFrame(), false);
  SetStorageForFrame(GetFrame());
  ExpectStorageForFrame(GetFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetFrame(), false);

  // Allow all requests to b.com to access storage.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL a_url = https_server_.GetURL("a.com", "/");
  GURL b_url = https_server_.GetURL("b.com", "/");
  cookie_settings->SetCookieSetting(b_url,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetFrame(), true);

  // Remove ALLOW setting.
  cookie_settings->ResetCookieSetting(b_url);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetFrame(), false);

  // Allow all third-parties on a.com to access storage.
  cookie_settings->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetFrame(), true);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyTopLevelOriginBrowserTest,
                       NestedThirdPartyIFrameStorage) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");

  ExpectStorageForFrame(GetNestedFrame(), false);
  SetStorageForFrame(GetNestedFrame());
  ExpectStorageForFrame(GetNestedFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), false);

  // Allow all requests to b.com to access storage.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL a_url = https_server_.GetURL("a.com", "/");
  GURL c_url = https_server_.GetURL("c.com", "/");
  cookie_settings->SetCookieSetting(c_url,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), true);

  // Remove ALLOW setting.
  cookie_settings->ResetCookieSetting(c_url);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), false);

  // Allow all third-parties on a.com to access storage.
  cookie_settings->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), true);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyTopLevelOriginBrowserTest,
                       NestedFirstPartyIFrameStorage) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("a.com", "/browsing_data/site_data.html");

  ExpectStorageForFrame(GetNestedFrame(), false);
  SetStorageForFrame(GetNestedFrame());
  ExpectStorageForFrame(GetNestedFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("a.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), false);

  // Allow all requests to b.com to access storage.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL a_url = https_server_.GetURL("a.com", "/");
  cookie_settings->SetCookieSetting(a_url,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("a.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), true);

  // Remove ALLOW setting.
  cookie_settings->ResetCookieSetting(a_url);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("a.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), false);

  // Allow all third-parties on a.com to access storage.
  cookie_settings->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("a.com", "/browsing_data/site_data.html");
  ExpectStorageForFrame(GetNestedFrame(), true);
}

}  // namespace
