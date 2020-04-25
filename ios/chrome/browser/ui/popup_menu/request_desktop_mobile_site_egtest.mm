// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kUserAgentTestURL[] =
    "http://ios/testing/data/http_server_files/user_agent_test_page.html";

const char kMobileSiteLabel[] = "Mobile";

const char kDesktopSiteLabel[] = "Desktop";
const char kDesktopPlatformLabel[] = "MacIntel";

// URL to be used when the page needs to be reloaded on back/forward
// navigations.
const char kPurgeURL[] = "url-purge.com";
// JavaScript used to reload the page on back/forward navigations.
const char kJavaScriptReload[] =
    "<script>window.onpageshow = function(event) {"
    "    if (event.persisted) {"
    "       window.location.href = window.location.href + \"?reloaded\""
    "    }"
    "};</script>";

// Custom timeout used when waiting for a web state after requesting desktop
// or mobile mode.
const NSTimeInterval kWaitForUserAgentChangeTimeout = 15.0;

// Select the button to request desktop site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestDesktopButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestDesktopId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuTableViewId)];
}

// Select the button to request mobile site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestMobileButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestMobileId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuTableViewId)];
}

// A ResponseProvider that provides user agent for httpServer request.
class UserAgentResponseProvider : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override { return true; }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    // Do not return anything if static plist file has been requested,
    // as plain text is not a valid property list content.
    if ([[base::SysUTF8ToNSString(request.url.spec()) pathExtension]
            isEqualToString:@"plist"]) {
      *headers =
          web::ResponseProvider::GetResponseHeaders("", net::HTTP_NO_CONTENT);
      return;
    }

    std::string purge_additions = "";
    if (request.url.path().find(kPurgeURL) != std::string::npos) {
      purge_additions = kJavaScriptReload;
    }

    *headers = web::ResponseProvider::GetDefaultResponseHeaders();
    std::string userAgent;
    std::string desktop_user_agent =
        web::BuildUserAgentFromProduct(web::UserAgentType::DESKTOP, "");
    if (request.headers.GetHeader("User-Agent", &userAgent) &&
        userAgent == desktop_user_agent) {
      response_body->assign(std::string("Desktop\n") + purge_additions);
    } else {
      response_body->assign(std::string("Mobile\n") + purge_additions);
    }
  }
};
}  // namespace

// Tests for the tools popup menu.
@interface RequestDesktopMobileSiteTestCase : ChromeTestCase
@end

@implementation RequestDesktopMobileSiteTestCase

// Tests that requesting desktop site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestDesktopSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site of a page works and the requested user
// agent is kept when restoring the session.
- (void)testRequestDesktopSiteKeptSessionRestoration {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Close all tabs and undo, trigerring a restoration.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Verify that desktop user agent propagates.
  if (@available(iOS 13, *)) {
    [ChromeEarlGreyUI openToolsMenu];
    [RequestMobileButton() assertWithMatcher:grey_notNil()];
    [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [RequestDesktopButton() assertWithMatcher:grey_notNil()];
    [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
  }
}

// Tests that requesting desktop site of a page works and desktop user agent
// does not propagate to next the new tab.
- (void)testRequestDesktopSiteDoesNotPropagateToNewTab {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent does not propagate to new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that requesting desktop site of a page works and going back re-opens
// mobile version of the page.
// TODO(crbug.com/990186): Re-enable this test.
- (void)DISABLED_testRequestDesktopSiteGoBackToMobile {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that when requesting desktop on another page and coming back to a page
// that has been purged from memory, we still display the mobile page.
- (void)testRequestDesktopSiteGoBackToMobilePurged {
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"On iOS 12, the User Agent can be wrong when "
                            @"doing back/forward navigations");
  }

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(
                              "http://" + std::string(kPurgeURL))];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForPageLoadTimeout,
                 ^bool {
                   return [ChromeEarlGrey webStateVisibleURL].query() ==
                          "reloaded";
                 }),
             @"Page did not reload");
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that requesting mobile site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestMobileSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that requesting mobile site of a page works and going back re-opens
// desktop version of the page.
// TODO(crbug.com/990186): Re-enable this test.
- (void)DISABLED_testRequestMobileSiteGoBackToDesktop {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that going back returns to the desktop site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site button is not enabled on new tab pages.
- (void)testRequestDesktopSiteNotEnabledOnNewTabPage {
  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
  [RequestDesktopButton() assertWithMatcher:grey_notNil()];
}

// Tests that requesting desktop site button is not enabled on WebUI pages.
- (void)testRequestDesktopSiteNotEnabledOnWebUIPage {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
  [RequestDesktopButton() assertWithMatcher:grey_notNil()];
}

// Tests that navigator.appVersion JavaScript API returns correct string for
// mobile User Agent and the platform.
- (void)testAppVersionJSAPIWithMobileUserAgent {
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUserAgentTestURL)];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  std::string platform =
      base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
  [ChromeEarlGrey waitForWebStateContainingText:platform];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];
  if (@available(iOS 13, *)) {
    [ChromeEarlGrey waitForWebStateContainingText:kDesktopPlatformLabel];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:platform];
  }

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:platform];
}

@end
