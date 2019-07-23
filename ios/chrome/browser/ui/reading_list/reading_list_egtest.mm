// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/features.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_identifiers.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/static_html_view_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;

namespace {
const char kContentToRemove[] = "Text that distillation should remove.";
const char kContentToKeep[] = "Text that distillation should keep.";
const char kDistillableTitle[] = "Tomato";
const char kDistillableURL[] = "/potato";
const char kNonDistillableURL[] = "/beans";
const char kReadTitle[] = "foobar";
const char kReadURL[] = "http://readfoobar.com";
const char kUnreadTitle[] = "I am an unread entry";
const char kUnreadURL[] = "http://unreadfoobar.com";
const char kReadURL2[] = "http://kReadURL2.com";
const char kReadTitle2[] = "read item 2";
const char kUnreadTitle2[] = "I am another unread entry";
const char kUnreadURL2[] = "http://unreadfoobar2.com";
const size_t kNumberReadEntries = 2;
const size_t kNumberUnreadEntries = 2;
const CFTimeInterval kSnackbarAppearanceTimeout = 5;
const CFTimeInterval kSnackbarDisappearanceTimeout =
    MDCSnackbarMessageDurationMax + 1;
const CFTimeInterval kDelayForSlowWebServer = 4;
const CFTimeInterval kLoadOfflineTimeout = kDelayForSlowWebServer + 1;
const CFTimeInterval kLongPressDuration = 1.0;
const CFTimeInterval kDistillationTimeout = 5;
const CFTimeInterval kServerOperationDelay = 1;
const char kReadHeader[] = "Read";
const char kUnreadHeader[] = "Unread";

// Overrides the NetworkChangeNotifier to enable distillation even if the device
// does not have network.
class WifiNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  WifiNetworkChangeNotifier() : net::NetworkChangeNotifier() {}

  ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_WIFI;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiNetworkChangeNotifier);
};

// Returns the string concatenated |n| times.
std::string operator*(const std::string& s, unsigned int n) {
  std::ostringstream out;
  for (unsigned int i = 0; i < n; i++)
    out << s;
  return out.str();
}

// Returns the reading list model.
ReadingListModel* GetReadingListModel() {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(2,
                                                   ^{
                                                     return model->loaded();
                                                   }),
      @"Reading List model did not load");
  return model;
}

// Scroll to the top of the Reading List.
void ScrollToTop() {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID([
                                          [ReadingListTableViewController class]
                                          accessibilityIdentifier])]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.5, 0.5)
              error:&error];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Asserts that the "mark" toolbar button is visible and has the a11y label of
// |a11y_label_id|.
void AssertToolbarMarkButtonText(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(kReadingListToolbarMarkButtonID),
              grey_ancestor(grey_kindOfClass([UIToolbar class])),
              chrome_test_util::ButtonWithAccessibilityLabelId(a11y_label_id),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts the |button_id| toolbar button is not visible.
void AssertToolbarButtonNotVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClass(
                                              [UIToolbar class])),
                                          nil)]
      assertWithMatcher:grey_notVisible()];
}

// Assert the |button_id| toolbar button is visible.
void AssertToolbarButtonVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClass(
                                              [UIToolbar class])),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Taps the |button_id| toolbar button.
void TapToolbarButtonWithID(NSString* button_id) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(button_id)]
      performAction:grey_tap()];
}

// Taps the context menu button with the a11y label of |a11y_label_id|.
void TapContextMenuButtonWithA11yLabelID(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   a11y_label_id)] performAction:grey_tap()];
}

// Performs |action| on the entry with the title |entryTitle|. The view can be
// scrolled down to find the entry.
void PerformActionOnEntry(const std::string& entryTitle,
                          id<GREYAction> action) {
  ScrollToTop();
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(entryTitle)),
                 grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [ReadingListTableViewController class])]
      performAction:action];
}

// Taps the entry with the title |entryTitle|.
void TapEntry(const std::string& entryTitle) {
  PerformActionOnEntry(entryTitle, grey_tap());
}

// Long-presses the entry with the title |entryTitle|.
void LongPressEntry(const std::string& entryTitle) {
  PerformActionOnEntry(entryTitle,
                       grey_longPressWithDuration(kLongPressDuration));
}

// Asserts that the entry with the title |entryTitle| is visible.
void AssertEntryVisible(const std::string& entryTitle) {
  ScrollToTop();
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(entryTitle)),
                     grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [[ReadingListTableViewController class]
                                   accessibilityIdentifier])]
      assertWithMatcher:grey_notNil()];
}

// Asserts that all the entries are visible.
void AssertAllEntriesVisible() {
  AssertEntryVisible(kReadTitle);
  AssertEntryVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);

  // If the number of entries changes, make sure this assert gets updated.
  GREYAssertEqual((size_t)2, kNumberReadEntries,
                  @"The number of entries have changed");
  GREYAssertEqual((size_t)2, kNumberUnreadEntries,
                  @"The number of entries have changed");
}

// Asserts that the entry |title| is not visible.
void AssertEntryNotVisible(std::string title) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  NSError* error;

  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(title)),
                     grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [[ReadingListTableViewController class]
                                   accessibilityIdentifier])]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertNotNil(error, @"Entry is visible");
}

// Asserts |header| is visible.
void AssertHeaderNotVisible(std::string header) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              base::SysUTF8ToNSString(header))]
      assertWithMatcher:grey_notVisible()];
}

// Opens the reading list menu using command line.
void OpenReadingList() {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() showReadingList];
}

// Adds a read and an unread entry to the model, opens the reading list menu and
// enter edit mode.
void AddEntriesAndEnterEdit() {
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(GURL(kReadURL), std::string(kReadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL), true);
  model->AddEntry(GURL(kReadURL2), std::string(kReadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL2), true);
  model->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->AddEntry(GURL(kUnreadURL2), std::string(kUnreadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  OpenReadingList();

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
}

// Computes the number of read entries in the model.
size_t ModelReadSize(ReadingListModel* model) {
  size_t size = 0;
  for (const auto& url : model->Keys()) {
    if (model->GetEntryByURL(url)->IsRead()) {
      size++;
    }
  }
  return size;
}

// Returns a match for the Reading List Empty Collection Background.
id<GREYMatcher> EmptyBackground() {
  return grey_accessibilityID(
      [[TableViewEmptyView class] accessibilityIdentifier]);
}

// Adds the current page to the Reading List.
void AddCurrentPageToReadingList() {
  // Add the page to the reading list.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::ButtonWithAccessibilityLabelId(
                             IDS_IOS_SHARE_MENU_READING_LIST_ACTION)];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  ConditionBlock wait_for_appearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Wait for the snackbar to disappear.
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarDisappearanceTimeout, wait_for_disappearance),
             @"Snackbar did not disappear.");
  if (net::NetworkChangeNotifier::IsOffline()) {
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }
}

// Wait until one element is distilled.
void WaitForDistillation() {
  NSString* a11y_id =
      [TableViewURLCellFaviconBadgeView accessibilityIdentifier];
  ConditionBlock wait_for_distillation_date = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(a11y_id)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kDistillationTimeout, wait_for_distillation_date),
             @"Item was not distilled.");
}

// Serves URLs. Response can be delayed by |delay| second or return an error if
// |responds_with_content| is false.
// If |distillable|, result is can be distilled for offline display.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryOrCloseSocket(
    const bool& responds_with_content,
    const int& delay,
    bool distillable,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
      base::TimeDelta::FromSeconds(delay));
  response->set_content_type("text/html");
  if (distillable) {
    std::string page_title = "Tomato";

    std::string content_to_remove(kContentToRemove);
    std::string content_to_keep(kContentToKeep);

    response->set_content("<html><head><title>" + page_title +
                          "</title></head>" + content_to_remove * 20 +
                          "<article>" + content_to_keep * 20 + "</article>" +
                          content_to_remove * 20 + "</html>");
  } else {
    response->set_content("<html><head><title>greens</title></head></html>");
  }
  return std::move(response);
}

// Opens the page security info bubble.
void OpenPageSecurityInfoBubble() {
  // In UI Refresh, the security info is accessed through the tools menu.
  [ChromeEarlGreyUI openToolsMenu];
  // Tap on the Page Info button.
  [ChromeEarlGreyUI
      tapToolsMenuButton:grey_accessibilityID(kToolsMenuSiteInformation)];
}

// Waits for a static html view containing |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
void WaitForStaticHtmlViewContainingText(NSString* text) {
  bool has_static_view =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return chrome_test_util::StaticHtmlViewContainingText(
            chrome_test_util::GetCurrentWebState(),
            base::SysNSStringToUTF8(text));
      });

  NSString* error_description = [NSString
      stringWithFormat:@"Failed to find static html view containing %@", text];
  GREYAssert(has_static_view, error_description);
}

// Waits for there to be no static html view, or a static html view that does
// not contain |text|. If the condition is not met within a timeout, a
// GREYAssert is induced.
void WaitForStaticHtmlViewNotContainingText(NSString* text) {
  bool no_static_view =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return !chrome_test_util::StaticHtmlViewContainingText(
            chrome_test_util::GetCurrentWebState(),
            base::SysNSStringToUTF8(text));
      });

  NSString* error_description = [NSString
      stringWithFormat:@"Failed, there was a static html view containing %@",
                       text];
  GREYAssert(no_static_view, error_description);
}

void AssertIsShowingDistillablePageNoNativeContent(
    bool online,
    const GURL& distillable_url) {
  [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          distillable_url.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Test that the offline and online pages are properly displayed.
  if (online) {
    [ChromeEarlGrey waitForWebStateContainingText:kContentToRemove];
    [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];
  } else {
    [ChromeEarlGrey waitForWebStateNotContainingText:kContentToRemove];
    [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];
  }

  // Test the presence of the omnibox offline chip.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImageNamed(
                                @"location_bar_offline"),
                            nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

void AssertIsShowingDistillablePageNativeContent(bool online,
                                                 const GURL& distillable_url) {
  NSString* contentToKeep = base::SysUTF8ToNSString(kContentToKeep);
  // There will be multiple reloads, wait for the page to be displayed.
  if (online) {
    // Due to the reloads, a timeout longer than what is provided in
    // [ChromeEarlGrey waitForWebStateContainingText] is required, so call
    // WebViewContainingText directly.
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   kLoadOfflineTimeout,
                   ^bool {
                     return web::test::IsWebViewContainingText(
                         chrome_test_util::GetCurrentWebState(),
                         kContentToKeep);
                   }),
               @"Waiting for online page.");
  } else {
    WaitForStaticHtmlViewContainingText(contentToKeep);
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          distillable_url.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Test that the offline and online pages are properly displayed.
  if (online) {
    [ChromeEarlGrey
        waitForWebStateContainingText:base::SysNSStringToUTF8(contentToKeep)];
    WaitForStaticHtmlViewNotContainingText(contentToKeep);
  } else {
    [ChromeEarlGrey waitForWebStateNotContainingText:kContentToKeep];
    WaitForStaticHtmlViewContainingText(contentToKeep);
  }

  // Test the presence of the omnibox offline chip.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImageNamed(
                                @"location_bar_offline"),
                            nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

// Tests that the correct version of kDistillableURL is displayed.
void AssertIsShowingDistillablePage(bool online, const GURL& distillable_url) {
  if (reading_list::IsOfflinePageWithoutNativeContentEnabled()) {
    return AssertIsShowingDistillablePageNoNativeContent(online,
                                                         distillable_url);
  }
  return AssertIsShowingDistillablePageNativeContent(online, distillable_url);
}

}  // namespace

// Test class for the Reading List menu.
@interface ReadingListTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) bool serverRespondsWithContent;

// The delay after which self.testServer will send a response.
@property(atomic) NSTimeInterval serverResponseDelay;
;
@end

@implementation ReadingListTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;
@synthesize serverResponseDelay = _serverResponseDelay;

- (void)setUp {
  [super setUp];
  ReadingListModel* model = GetReadingListModel();
  for (const GURL& url : model->Keys())
    model->RemoveEntryByURL(url);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kDistillableURL,
      base::BindRepeating(&HandleQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent),
                          std::cref(_serverResponseDelay), true)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kNonDistillableURL,
      base::BindRepeating(&HandleQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent),
                          std::cref(_serverResponseDelay), false)));
  self.serverRespondsWithContent = true;
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the Reading List view is accessible.
- (void)testAccessibility {
  AddEntriesAndEnterEdit();
  // In edit mode.
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  TapToolbarButtonWithID(kReadingListToolbarCancelButtonID);
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version via context menu.
- (void)testSavingToReadingListAndLoadDistilled {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  GURL nonDistillablePageURL(self.testServer->GetURL(kNonDistillableURL));
  std::string pageTitle(kDistillableTitle);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillablePageURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:nonDistillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);

  WaitForDistillation();

  // Long press the entry, and open it offline.
  LongPressEntry(pageTitle);
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE);
  AssertIsShowingDistillablePage(false, distillablePageURL);

  // Tap the Omnibox' Info Bubble to open the Page Info.
  OpenPageSecurityInfoBubble();
  // Verify that the Page Info is about offline pages.
  id<GREYMatcher> pageInfoTitleMatcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_PAGE_INFO_OFFLINE_TITLE);
  [[EarlGrey selectElementWithMatcher:pageInfoTitleMatcher]
      assertWithMatcher:grey_notNil()];

  // Verify that the webState's title is correct.
  XCTAssertTrue(chrome_test_util::GetCurrentWebState()->GetTitle() ==
                base::ASCIIToUTF16(pageTitle.c_str()));
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads online version by tapping on entry.
- (void)testSavingToReadingListAndLoadNormal {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  std::string pageTitle(kDistillableTitle);
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  // Press the entry, and open it online.
  TapEntry(pageTitle);

  AssertIsShowingDistillablePage(true, distillableURL);
  // Stop server to reload offline.
  self.serverRespondsWithContent = NO;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));

  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(false, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry without web server.
- (void)testSavingToReadingListAndLoadNoNetwork {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  std::string pageTitle(kDistillableTitle);
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  // Stop server to generate error.
  self.serverRespondsWithContent = NO;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));
  // Long press the entry, and open it offline.
  TapEntry(pageTitle);
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload. As server is still down, the offline page should show again.
  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(false, distillableURL);

  // TODO(crbug.com/954248) This DCHECK's (but works) with slimnav disabled.
  if ([ChromeEarlGrey isSlimNavigationManagerEnabled]) {
    [ChromeEarlGrey goBack];
    [ChromeEarlGrey goForward];
    AssertIsShowingDistillablePage(false, distillableURL);
  }

  // Start server to reload online error.
  self.serverRespondsWithContent = YES;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));

  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(true, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry with delayed web server.
- (void)testSavingToReadingListAndLoadBadNetwork {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  std::string pageTitle(kDistillableTitle);
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  self.serverResponseDelay = kDelayForSlowWebServer;
  // Open the entry.
  TapEntry(pageTitle);

  AssertIsShowingDistillablePage(false, distillableURL);

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload should load online page.
  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(true, distillableURL);
  // Reload should load offline page.
  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(false, distillableURL);
}

// Tests that only the "Edit" button is showing when not editing.
- (void)testVisibleButtonsNonEditingMode {
  GetReadingListModel()->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                                  reading_list::ADDED_VIA_CURRENT_APP);
  OpenReadingList();

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarMarkButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarEditButtonID);
}

// Tests that only the "Cancel", "Delete All Read" and "Mark All…" buttons are
// showing when not editing.
- (void)testVisibleButtonsEditingModeEmptySelection {
  AddEntriesAndEnterEdit();

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Unread" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyReadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Read" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyUnreadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark…" buttons are showing when
// not editing.
- (void)testVisibleButtonsMixedEntriesSelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
}

// Tests the deletion of selected entries.
- (void)testDeleteEntries {
  AddEntriesAndEnterEdit();

  TapEntry(kReadTitle2);

  TapToolbarButtonWithID(kReadingListToolbarDeleteButtonID);

  AssertEntryVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
}

// Tests the deletion of all read entries.
- (void)testDeleteAllReadEntries {
  AddEntriesAndEnterEdit();

  TapToolbarButtonWithID(kReadingListToolbarDeleteAllReadButtonID);

  AssertEntryNotVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertHeaderNotVisible(kReadHeader);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
}

// Marks all unread entries as read.
- (void)testMarkAllRead {
  AddEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION);

  AssertHeaderNotVisible(kUnreadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 ModelReadSize(GetReadingListModel()));
  XCTAssertEqual((size_t)0, GetReadingListModel()->unread_size());
}

// Marks all read entries as unread.
- (void)testMarkAllUnread {
  AddEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);

  AssertHeaderNotVisible(kReadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 GetReadingListModel()->unread_size());
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
}

// Selects an unread entry and mark it as read.
- (void)testMarkEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

// Selects an read entry and mark it as unread.
- (void)testMarkEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
}

// Selects read and unread entries and mark them as unread.
- (void)testMarkMixedEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
}

// Selects read and unread entries and mark them as read.
- (void)testMarkMixedEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

// Tests that you can delete multiple read items in the Reading List without
// creating a crash (crbug.com/701956).
- (void)testDeleteMultipleItems {
  // Add entries.
  ReadingListModel* model = GetReadingListModel();
  for (int i = 0; i < 11; i++) {
    std::string increment = std::to_string(i);
    model->AddEntry(GURL(kReadURL + increment),
                    std::string(kReadTitle + increment),
                    reading_list::ADDED_VIA_CURRENT_APP);
    model->SetReadStatus(GURL(kReadURL + increment), true);
  }

  // Delete them from the Reading List view.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:EmptyBackground()]
      assertWithMatcher:grey_nil()];
  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
  TapToolbarButtonWithID(kReadingListToolbarDeleteAllReadButtonID);

  // Verify the background string is displayed.
  [[EarlGrey selectElementWithMatcher:EmptyBackground()]
      assertWithMatcher:grey_notNil()];
}

@end
