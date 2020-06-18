// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class NavigationNotificationObserver : public content::NotificationObserver {
 public:
  NavigationNotificationObserver()
      : got_navigation_(false),
        http_status_code_(0) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
    got_navigation_ = true;
    http_status_code_ =
        content::Details<content::LoadCommittedDetails>(details)->
        http_status_code;
  }

  int http_status_code() const { return http_status_code_; }
  bool got_navigation() const { return got_navigation_; }

 private:
  content::NotificationRegistrar registrar_;
  int got_navigation_;
  int http_status_code_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class NavigationObserver : public content::WebContentsObserver {
public:
  enum NavigationResult {
    NOT_FINISHED,
    ERROR_PAGE,
    SUCCESS,
  };

  explicit NavigationObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents), navigation_result_(NOT_FINISHED) {}
  ~NavigationObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_result_ =
        navigation_handle->IsErrorPage() ? ERROR_PAGE : SUCCESS;
    net_error_ = navigation_handle->GetNetErrorCode();
  }

  NavigationResult navigation_result() const { return navigation_result_; }
  net::Error net_error() const { return net_error_; }

  void Reset() {
    navigation_result_ = NOT_FINISHED;
    net_error_ = net::OK;
  }

 private:
  NavigationResult navigation_result_;
  net::Error net_error_ = net::OK;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

}  // namespace

typedef InProcessBrowserTest ChromeURLDataManagerTest;

// Makes sure navigating to the new tab page results in a http status code
// of 200.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, 200) {
  NavigationNotificationObserver observer;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(observer.got_navigation());
  EXPECT_EQ(200, observer.http_status_code());
}

// Makes sure browser does not crash when navigating to an unknown resource.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, UnknownResource) {
  // Known resource
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON"));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unknown resource
  observer.Reset();
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_ASDFGHJKL"));
  EXPECT_EQ(NavigationObserver::ERROR_PAGE, observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

// Makes sure browser does not crash when the resource scale is very large.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, LargeResourceScale) {
  // Valid scale
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@2x"));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unreasonably large scale
  observer.Reset();
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@99999x"));
  EXPECT_EQ(NavigationObserver::ERROR_PAGE, observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

class ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled() {
    feature_list_.InitAndEnableFeature(features::kWebUIReportOnlyTrustedTypes);
  }

  void CheckTrustedTypesViolation(base::StringPiece url) {
    std::string message_filter = "*This document requires*assignment*";
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetPattern(message_filter);

    ASSERT_TRUE(embedded_test_server()->Start());
    ui_test_utils::NavigateToURL(browser(), GURL(url));

    // Round trip to the renderer to ensure that the page is loaded
    EXPECT_TRUE(content::ExecuteScript(content, "var a = 0;"));
    EXPECT_TRUE(console_observer.messages().empty());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that there's no Trusted Types violation in chrome://chrome-urls
IN_PROC_BROWSER_TEST_P(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolation) {
  CheckTrustedTypesViolation(GetParam());
}

// Non-exhaustive list of chrome:// URLs to test for trusted types violations.
// This list was derived from chrome://about. :)
static constexpr const char* const kChromeUrls[] = {
    "chrome://accessibility",
    "chrome://autofill-internals",
    "chrome://blob-internals",
    "chrome://bluetooth-internals",
    "chrome://chrome-urls",
    "chrome://components",
    "chrome://conflicts",
    "chrome://crashes",
    "chrome://credits",
    "chrome://cryptohome",
    "chrome://device-log",
    "chrome://devices",
    "chrome://download-internals",
    "chrome://drive-internals",
    "chrome://explore-sites-internals",
    "chrome://first-run",
    "chrome://flags",
    "chrome://gcm-internals",
    "chrome://gpu",
    "chrome://histograms",
    "chrome://indexeddb-internals",
    "chrome://inspect",
    "chrome://interventions-internals",
    "chrome://invalidations",
    "chrome://linux-proxy-config",
    "chrome://local-state",
    "chrome://machine-learning-internals",
    "chrome://media-engagement",
    "chrome://media-internals",
    "chrome://nacl",
    "chrome://net-export",
    "chrome://network-errors",
    "chrome://ntp-tiles-internals",
    "chrome://omnibox",
    "chrome://password-manager-internals",
    "chrome://policy",
    "chrome://power",
    "chrome://predictors",
    "chrome://prefs-internals",
    "chrome://process-internals",
    "chrome://quota-internals",
    "chrome://safe-browsing",
    "chrome://sandbox",
    "chrome://serviceworker-internals",
    "chrome://signin-internals",
    "chrome://site-engagement",
    "chrome://snippets-internals",
    "chrome://suggestions",
    "chrome://supervised-user-internals",
    "chrome://sync-internals",
    "chrome://system",
    "chrome://terms",
    "chrome://translate-internals",
    "chrome://usb-internals",
    "chrome://user-actions",
    "chrome://version",
    "chrome://webapks",
    "chrome://webrtc-internals",
    "chrome://webrtc-logs",
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    ::testing::ValuesIn(kChromeUrls));
