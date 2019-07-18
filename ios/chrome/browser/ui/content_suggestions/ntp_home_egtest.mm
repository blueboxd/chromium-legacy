// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include <memory>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/notification_promo.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/base/scoped_block_swizzler.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace content_suggestions;
using namespace ntp_home;
using namespace ntp_snippets;

namespace {
const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}
}

// Test case for the NTP home UI. More precisely, this tests the positions of
// the elements after interacting with the device.
@interface NTPHomeTestCase : ChromeTestCase

// Current non-incognito browser state.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
// Mock provider from the singleton.
@property(nonatomic, assign, readonly) MockContentSuggestionsProvider* provider;
// Article category, used by the singleton.
@property(nonatomic, assign, readonly) ntp_snippets::Category category;

@property(nonatomic, assign) base::string16 defaultSearchEngine;

@end

@implementation NTPHomeTestCase

+ (void)setUp {
  [super setUp];

  // Clear the pasteboard in case there is a URL copied, triggering an omnibox
  // suggestion.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)tearDown {
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  ReadingListModelFactory::GetForBrowserState(browserState)->DeleteAllEntries();

  // Resets the Service associated with this browserState to a new service with
  // no providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));
  [super tearDown];
}

- (void)setUp {
  self.provider->FireCategoryStatusChanged(self.category,
                                           CategoryStatus::AVAILABLE);

  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingListModel->DeleteAllEntries();
  [super setUp];

  // Get the default Search Engine.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  self.defaultSearchEngine = service->GetDefaultSearchProvider()->short_name();
}

- (void)tearDown {
  self.provider->FireCategoryStatusChanged(
      self.category, CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];

  // Set the search engine back to the default in case the test fails before
  // cleaning it up.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  std::vector<TemplateURL*> urls = service->GetTemplateURLs();

  for (auto iter = urls.begin(); iter != urls.end(); ++iter) {
    if (self.defaultSearchEngine == (*iter)->short_name()) {
      service->SetUserSelectedDefaultSearchProvider(*iter);
    }
  }

  [super tearDown];
}

#pragma mark - Properties

- (ios::ChromeBrowserState*)browserState {
  return chrome_test_util::GetOriginalBrowserState();
}

- (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

- (ntp_snippets::Category)category {
  return ntp_snippets::Category::FromKnownCategory(KnownCategories::ARTICLES);
}

#pragma mark - Tests

// Tests that all items are accessible on the home page.
- (void)testAccessibility {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the collections shortcut are displayed and working.
- (void)testCollectionShortcuts {
  // Check the Bookmarks.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the ReadingList.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_READING_LIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the RecentTabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the History.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_HISTORY)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_HISTORY_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that the fake omnibox width is correctly updated after a rotation.
- (void)testOmniboxWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  UIEdgeInsets safeArea = CollectionView().safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(CollectionView().bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = searchFieldWidth(collectionWidth);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  safeArea = CollectionView().safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(CollectionView().bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = searchFieldWidth(collectionWidthAfterRotation);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the settings screen is shown.
- (void)testOmniboxWidthRotationBehindSettings {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsRegularXRegularSizeClass()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  UIEdgeInsets safeArea = CollectionView().safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(CollectionView().bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = searchFieldWidth(collectionWidth);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [ChromeEarlGreyUI openSettingsMenu];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  safeArea = CollectionView().safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(CollectionView().bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = searchFieldWidth(collectionWidthAfterRotation);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the fake omnibox is pinned to the top.
- (void)testOmniboxPinnedWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsRegularXRegularSizeClass()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidth = CollectionView().bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");

  // The fake omnibox might be slightly bigger than the screen in order to cover
  // it for all screen scale.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidthBetween(collectionWidth + 1, 2)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidthAfterRotation = CollectionView().bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that the app doesn't crash when opening multiple tabs.
- (void)testOpenMultipleTabs {
  NSInteger numberOfTabs = 10;
  for (NSInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGreyUI openNewTab];
  }
  id<GREYMatcher> matcher;
  if ([ChromeEarlGrey isIPadIdiom]) {
    matcher = grey_accessibilityID(@"Enter Tab Switcher");
  } else {
    matcher = grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                         grey_sufficientlyVisible(), nil);
  }
  [[EarlGrey selectElementWithMatcher:matcher]
      assertWithMatcher:grey_accessibilityValue([NSString
                            stringWithFormat:@"%@", @(numberOfTabs + 1)])];
}

// Tests that the promo is correctly displayed and removed once tapped.
- (void)testPromoTap {
  // Setup the promo.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:experimental_flags::WHATS_NEW_MOVE_TO_DOCK_TIP
                forKey:@"WhatsNewPromoStatus"];
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  ios::NotificationPromo::MigrateUserPrefs(local_state);

  // Open a new tab to have the promo.
  [ChromeEarlGreyUI openNewTab];

  // Tap the promo.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          [ContentSuggestionsWhatsNewItem
                                              accessibilityIdentifier])]
      performAction:grey_tap()];

  // Promo dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          [ContentSuggestionsWhatsNewItem
                                              accessibilityIdentifier])]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Reset the promo.
  [defaults setInteger:experimental_flags::WHATS_NEW_DEFAULT
                forKey:@"WhatsNewPromoStatus"];
  ios::NotificationPromo::MigrateUserPrefs(local_state);
}

// Tests that the position of the collection view is restored when navigating
// back to the NTP.
- (void)testPositionRestored {
  [self addMostVisitedTile];

  // Add suggestions to be able to scroll on iPad.
  ReadingListModelFactory::GetForBrowserState(self.browserState)
      ->AddEntry(GURL("http://chromium.org/"), "title",
                 reading_list::ADDED_VIA_CURRENT_APP);
  self.provider->FireSuggestionsChanged(self.category, ntp_home::Suggestions());

  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];

  // Save the position before navigating.
  UIView* omnibox = ntp_home::FakeOmnibox();
  CGPoint previousPosition = omnibox.bounds.origin;

  // Navigate and come back.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  omnibox = ntp_home::FakeOmnibox();
  GREYAssertEqual(previousPosition.y, omnibox.bounds.origin.y,
                  @"Omnibox not at the same position");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// and moved up, the scroll position restored is the position before the omnibox
// is selected.
- (void)testPositionRestoredWithOmniboxFocused {
  [self addMostVisitedTile];

  // Add suggestions to be able to scroll on iPad.
  ReadingListModelFactory::GetForBrowserState(self.browserState)
      ->AddEntry(GURL("http://chromium.org/"), "title",
                 reading_list::ADDED_VIA_CURRENT_APP);
  self.provider->FireSuggestionsChanged(self.category, ntp_home::Suggestions());

  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];

  // Save the position before navigating.
  UIView* omnibox = ntp_home::FakeOmnibox();
  CGPoint previousPosition = omnibox.bounds.origin;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Navigate and come back.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  omnibox = ntp_home::FakeOmnibox();
  GREYAssertEqual(previousPosition.y, omnibox.bounds.origin.y,
                  @"Omnibox not at the same position");
}

// Tests that tapping the fake omnibox focuses the real omnibox.
- (void)testTapFakeOmnibox {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if (IsRegularXRegularSizeClass()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  // Setup the server.
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  NSString* URL = base::SysUTF8ToNSString(pageURL.spec());
  // Tap the fake omnibox, type the URL in the real omnibox and navigate to the
  // page.
  [self focusFakebox];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([URL stringByAppendingString:@"\n"])];

  // Check that the page is loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
}

// Tests that tapping the omnibox search button logs correctly.
// It is important for ranking algorithm of omnibox that requests from the
// search button and real omnibox are marked appropriately.
- (void)testTapOmniboxSearchButtonLogsCorrectly {
  if (IsRegularXRegularSizeClass()) {
    // This logging only happens on iPhone, since on iPad there's no secondary
    // toolbar.
    return;
  }

  // Swizzle the method that needs to be called for correct logging.
  __block BOOL tapped = NO;
  ScopedBlockSwizzler swizzler([LocationBarCoordinator class],
                               @selector(focusOmniboxFromSearchButton), ^{
                                 tapped = YES;
                               });

  // Tap the search button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolbarSearchButtonIdentifier)]
      performAction:grey_tap()];

  // Check that the page is loaded.
  GREYAssertTrue(tapped,
                 @"The tap on the search button was not correctly logged.");
}

// Tests that tapping the fake omnibox moves the collection.
- (void)testTapFakeOmniboxScroll {
  // Get the collection and its layout.
  UIView* collection = ntp_home::CollectionView();
  GREYAssertTrue([collection isKindOfClass:[UICollectionView class]],
                 @"The collection has not been correctly selected.");
  UICollectionView* collectionView = (UICollectionView*)collection;
  GREYAssertTrue(
      [collectionView.delegate
          conformsToProtocol:@protocol(UICollectionViewDelegateFlowLayout)],
      @"The collection has not the expected delegate.");
  id<UICollectionViewDelegateFlowLayout> delegate =
      (id<UICollectionViewDelegateFlowLayout>)(collectionView.delegate);
  CGFloat headerHeight =
      [delegate collectionView:collectionView
                                   layout:collectionView.collectionViewLayout
          referenceSizeForHeaderInSection:0]
          .height;

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Offset after the fake omnibox has been tapped.
  CGPoint offsetAfterTap = collectionView.contentOffset;

  // Make sure the fake omnibox has been hidden and the collection has moved.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  CGFloat top = ntp_home::CollectionView().safeAreaInsets.top;
  GREYAssertTrue(offsetAfterTap.y >= origin.y + headerHeight - (60 + top),
                 @"The collection has not moved.");

  // Unfocus the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_tapAtPoint(CGPointMake(0, offsetAfterTap.y + 100))];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertEqual(
      origin.y, collectionView.contentOffset.y,
      @"The collection is not scrolled back to its previous position");
}

// Tests that tapping the fake omnibox then unfocusing it moves the collection
// back to where it was.
- (void)testTapFakeOmniboxScrollScrolled {
  // Get the collection and its layout.
  UIView* collection = ntp_home::CollectionView();
  GREYAssertTrue([collection isKindOfClass:[UICollectionView class]],
                 @"The collection has not been correctly selected.");
  UICollectionView* collectionView = (UICollectionView*)collection;

  // Scroll to have a position different from the default.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Unfocus the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_tapAtPoint(
                        CGPointMake(0, collectionView.contentOffset.y + 100))];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The collection might be slightly moved on iPhone.
  GREYAssertTrue(
      collectionView.contentOffset.y >= origin.y &&
          collectionView.contentOffset.y <= origin.y + 6,
      @"The collection is not scrolled back to its previous position");
}

// Tests tapping the search button when the fake omnibox is scrolled.
- (void)testTapSearchButtonFakeOmniboxScrolled {
  if (IsRegularXRegularSizeClass()) {
    // This only happens on iPhone, since on iPad there's no secondary toolbar.
    return;
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ContentSuggestionCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  // Tap the search button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolbarSearchButtonIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

- (void)testOpeningNewTab {
  [ChromeEarlGreyUI openNewTab];

  // Check that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  id<GREYMatcher> tabGridMatcher = nil;
  if ([ChromeEarlGrey isIPadIdiom]) {
    tabGridMatcher = grey_accessibilityID(@"Enter Tab Switcher");
  } else {
    tabGridMatcher =
        grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                   grey_sufficientlyVisible(), nil);
  }
  [[EarlGrey selectElementWithMatcher:tabGridMatcher]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 2])];

  // Test the same thing after opening a tab from the tab grid.
  // TODO(crbug.com/933953) For an unknown reason synchronization doesn't work
  // well with tapping on the tabgrid button, and instead triggers the long
  // press gesture recognizer.  Disable this here so the test can be re-enabled.
  {
    ScopedSynchronizationDisabler disabler;
    [[EarlGrey selectElementWithMatcher:tabGridMatcher]
        performAction:[GREYActions actionForLongPressWithDuration:0.05]];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:tabGridMatcher]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 3])];
}

- (void)testFavicons {
  for (NSInteger index = 0; index < 8; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Yahoo!.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Yahoo!")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Check again the favicons.
  for (NSInteger index = 0; index < 8; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Google.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Google")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Helpers

- (void)addMostVisitedTile {
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Clear history to ensure the tile will be shown.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

@end
