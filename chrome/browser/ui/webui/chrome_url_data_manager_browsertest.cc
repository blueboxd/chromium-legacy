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
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInChromeUrls) {
  CheckTrustedTypesViolation("chrome://chrome-urls");
}

// Verify that there's no Trusted Types violation in chrome://blob-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInBlobInternals) {
  CheckTrustedTypesViolation("chrome://blob-internals");
}

// Verify that there's no Trusted Types violation in chrome://device-log
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInDeviceLog) {
  CheckTrustedTypesViolation("chrome://device-log");
}

// Verify that there's no Trusted Types violation in chrome://devices
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInDevices) {
  CheckTrustedTypesViolation("chrome://devices");
}

// Verify that there's no Trusted Types violation in chrome://gcm-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInGCMInternals) {
  CheckTrustedTypesViolation("chrome://gcm-internals");
}

// Verify that there's no Trusted Types violation in chrome://inspect
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInInspect) {
  CheckTrustedTypesViolation("chrome://inspect");
}

// Verify that there's no Trusted Types violation in chrome://local-state
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInLocalState) {
  CheckTrustedTypesViolation("chrome://local-state");
}

// Verify that there's no Trusted Types violation in chrome://net-export
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInNetExport) {
  CheckTrustedTypesViolation("chrome://net-export");
}

// Verify that there's no Trusted Types violation in chrome://policy
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInPolicy) {
  CheckTrustedTypesViolation("chrome://policy");
}

// Verify that there's no Trusted Types violation in chrome://predictors
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInPredictors) {
  CheckTrustedTypesViolation("chrome://predictors");
}

// Verify that there's no Trusted Types violation in chrome://prefs-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInPrefsInternals) {
  CheckTrustedTypesViolation("chrome://prefs-internals");
}

// Verify that there's no Trusted Types violation in chrome://sandbox
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInSandbox) {
  CheckTrustedTypesViolation("chrome://sandbox");
}

// Verify that there's no Trusted Types violation in chrome://suggestions
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInSuggestions) {
  CheckTrustedTypesViolation("chrome://suggestions");
}

// Verify that there's no Trusted Types violation in chrome://terms
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInTerms) {
  CheckTrustedTypesViolation("chrome://terms");
}

// Verify that there's no Trusted Types violation in chrome://user-actions
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInUserActions) {
  CheckTrustedTypesViolation("chrome://user-actions");
}

// Verify that there's no Trusted Types violation in chrome://webrtc-logs
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInWebrtcLogs) {
  CheckTrustedTypesViolation("chrome://webrtc-logs");
}

// Verify that there's no Trusted Types violation in chrome://autofill-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInAutofillInternals) {
  CheckTrustedTypesViolation("chrome://autofill-internals");
}

// Verify that there's no Trusted Types violation in
// chrome://password-manager-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInPasswordManagerInternals) {
  CheckTrustedTypesViolation("chrome://password-manager-internals");
}

// Verify that there's no Trusted Types violation in chrome://media-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInMediaInternals) {
  CheckTrustedTypesViolation("chrome://media-internals");
}

// Verify that there's no Trusted Types violation in chrome://histograms
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInHistograms) {
  CheckTrustedTypesViolation("chrome://histograms");
}

// Verify that there's no Trusted Types violation in chrome://accessibility
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInAccessibility) {
  CheckTrustedTypesViolation("chrome://accessibility");
}

// Verify that there's no Trusted Types violation in chrome://process-internals
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInProcessInternals) {
  CheckTrustedTypesViolation("chrome://process-internals");
}

// Verify that there's no Trusted Types violation in chrome://credits
IN_PROC_BROWSER_TEST_F(
    ChromeURLDataManagerTestWithWebUIReportOnlyTrustedTypesEnabled,
    NoTrustedTypesViolationInCredits) {
  CheckTrustedTypesViolation("chrome://credits");
}
