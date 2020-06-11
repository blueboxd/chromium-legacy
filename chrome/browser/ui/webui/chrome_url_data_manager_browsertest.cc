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
    : public InProcessBrowserTest {
 public:
  ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled() {
    feature_list_.InitAndEnableFeature(features::kWebUIReportOnlyTrustedTypes);
  }

  // Verify that there's no Trusted Types violation in the given WebUI page.
  void CheckTrustedTypesViolation(base::StringPiece url) {
    std::string message_filter = "*This document requires*assignment*";
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetPattern(message_filter);
    ui_test_utils::NavigateToURL(browser(), GURL(url));

    // Round trip to the renderer to ensure that the page is loaded
    EXPECT_TRUE(content::ExecuteScript(content, "var a = 0;"));
    EXPECT_TRUE(console_observer.messages().empty());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Following tests are grouped to reduce the size of this file. We only
// allow 20 calls to CheckTrustedTypesViolation per test so that we won't
// cause timeout.
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInWebUIGroupA) {
  CheckTrustedTypesViolation("chrome://accessibility");
  CheckTrustedTypesViolation("chrome://autofill-internals");
  CheckTrustedTypesViolation("chrome://blob-internals");
  CheckTrustedTypesViolation("chrome://bluetooth-internals");
  CheckTrustedTypesViolation("chrome://chrome-urls");
  CheckTrustedTypesViolation("chrome://components");
  CheckTrustedTypesViolation("chrome://conflicts");
  CheckTrustedTypesViolation("chrome://crashes");
  CheckTrustedTypesViolation("chrome://credits");
  CheckTrustedTypesViolation("chrome://cryptohome");
  CheckTrustedTypesViolation("chrome://device-log");
  CheckTrustedTypesViolation("chrome://devices");
  CheckTrustedTypesViolation("chrome://download-internals");
  CheckTrustedTypesViolation("chrome://drive-internals");
  CheckTrustedTypesViolation("chrome://explore-sites-internals");
  CheckTrustedTypesViolation("chrome://first-run");
  CheckTrustedTypesViolation("chrome://flags");
  CheckTrustedTypesViolation("chrome://gcm-internals");
  CheckTrustedTypesViolation("chrome://gpu");
  CheckTrustedTypesViolation("chrome://histograms");
}

IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInWebUIGroupB) {
  CheckTrustedTypesViolation("chrome://indexeddb-internals");
  CheckTrustedTypesViolation("chrome://inspect");
  CheckTrustedTypesViolation("chrome://interventions-internals");
  CheckTrustedTypesViolation("chrome://invalidations");
  CheckTrustedTypesViolation("chrome://linux-proxy-config");
  CheckTrustedTypesViolation("chrome://local-state");
  CheckTrustedTypesViolation("chrome://machine-learning-internals");
  CheckTrustedTypesViolation("chrome://media-engagement");
  CheckTrustedTypesViolation("chrome://media-internals");
  CheckTrustedTypesViolation("chrome://nacl");
  CheckTrustedTypesViolation("chrome://net-export");
  CheckTrustedTypesViolation("chrome://network-errors");
  CheckTrustedTypesViolation("chrome://ntp-tiles-internals");
  CheckTrustedTypesViolation("chrome://omnibox");
  CheckTrustedTypesViolation("chrome://password-manager-internals");
  CheckTrustedTypesViolation("chrome://policy");
  CheckTrustedTypesViolation("chrome://power");
  CheckTrustedTypesViolation("chrome://predictors");
  CheckTrustedTypesViolation("chrome://prefs-internals");
  CheckTrustedTypesViolation("chrome://process-internals");
}

IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInWebUIGroupC) {
  CheckTrustedTypesViolation("chrome://quota-internals");
  CheckTrustedTypesViolation("chrome://safe-browsing");
  CheckTrustedTypesViolation("chrome://sandbox");
  CheckTrustedTypesViolation("chrome://serviceworker-internals");
  CheckTrustedTypesViolation("chrome://signin-internals");
  CheckTrustedTypesViolation("chrome://site-engagement");
  CheckTrustedTypesViolation("chrome://snippets-internals");
  CheckTrustedTypesViolation("chrome://suggestions");
  CheckTrustedTypesViolation("chrome://supervised-user-internals");
  CheckTrustedTypesViolation("chrome://sync-internals");
  CheckTrustedTypesViolation("chrome://system");
  CheckTrustedTypesViolation("chrome://terms");
  CheckTrustedTypesViolation("chrome://translate-internals");
  CheckTrustedTypesViolation("chrome://usb-internals");
  CheckTrustedTypesViolation("chrome://user-actions");
  CheckTrustedTypesViolation("chrome://version");
  CheckTrustedTypesViolation("chrome://webapks");
  CheckTrustedTypesViolation("chrome://webrtc-internals");
  CheckTrustedTypesViolation("chrome://webrtc-logs");
}
