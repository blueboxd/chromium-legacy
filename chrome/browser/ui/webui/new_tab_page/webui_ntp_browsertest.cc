// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

class WebUiNtpBrowserTest : public InProcessBrowserTest {
 public:
  WebUiNtpBrowserTest() {
    feature_list_.InitAndEnableFeature(ntp_features::kWebUI);
  }
  ~WebUiNtpBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the WebUI NTP commits in a SiteInstance with the WebUI URL.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, VerifyWebUiNtpSiteInstance) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), ntp_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  GURL webui_ntp_url(chrome::kChromeUINewTabPageURL);
  ASSERT_EQ(webui_ntp_url,
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
}

// Verify that the WebUI NTP uses process-per-site.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, ProcessPerSite) {
  std::vector<content::WebContents*> tabs;

  // Open a few NTPs.
  for (size_t i = 0; i < 3; i++) {
    content::WebContentsAddedObserver tab_observer;
    chrome::NewTab(browser());

    // Wait for the new tab.
    auto* tab = tab_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab));

    // Sanity check: the NTP should be a WebUI NTP (and not chrome://newtab/ or
    // some other NTP).
    EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL).spec(),
              EvalJs(tab, "window.location.href",
                     content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                     /*world_id=*/1));

    tabs.push_back(tab);
  }

  // Verify that all NTPs share a process.
  for (size_t i = 1; i < tabs.size(); i++) {
    EXPECT_EQ(tabs[0]->GetMainFrame()->GetProcess(),
              tabs[i]->GetMainFrame()->GetProcess());
  }
}
