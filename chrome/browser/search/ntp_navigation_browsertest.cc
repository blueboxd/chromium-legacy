// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

#include "chrome/browser/search/ntp_features.h"

class NtpNavigationBrowserTest : public InProcessBrowserTest {
 public:
  NtpNavigationBrowserTest() = default;
  ~NtpNavigationBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }
};

// Verify that the local NTP commits in a SiteInstance with the local NTP URL.
IN_PROC_BROWSER_TEST_F(NtpNavigationBrowserTest, VerifyNtpSiteInstance) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), ntp_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  GURL local_ntp_url(base::StrCat({chrome::kChromeSearchScheme, "://",
                                   chrome::kChromeSearchLocalNtpHost, "/"}));
  ASSERT_EQ(local_ntp_url,
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
}

// Subclass to allow running tests against the WebUI NTP implementation.
class WebUiNtpNavigationBrowserTest : public NtpNavigationBrowserTest {
 public:
  WebUiNtpNavigationBrowserTest() {
    feature_list_.InitAndEnableFeature(ntp_features::kWebUI);
  }
  ~WebUiNtpNavigationBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the WebUI NTP commits in a SiteInstance with the WebUI URL.
IN_PROC_BROWSER_TEST_F(WebUiNtpNavigationBrowserTest,
                       VerifyWebUiNtpSiteInstance) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), ntp_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  GURL webui_ntp_url(chrome::kChromeUINewTabPageURL);
  ASSERT_EQ(webui_ntp_url,
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
}
