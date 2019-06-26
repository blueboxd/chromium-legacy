// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "base/test/ios/wait_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/payments/core/features.h"
#import "components/ukm/ios/features.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#import "services/metrics/public/cpp/ukm_recorder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::BrowserCommandDispatcherForMainBVC;

@implementation ChromeEarlGreyAppInterface

+ (NSError*)clearBrowsingHistory {
  if (chrome_test_util::ClearBrowsingHistory()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser history timed out");
}

+ (void)startLoadingURL:(NSString*)spec {
  chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(spec)));
}

+ (BOOL)waitForWindowIDInjectionIfNeeded {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  if (webState->ContentIsHTML()) {
    return web::WaitUntilWindowIdInjected(webState);
  }

  return YES;
}

+ (bool)isLoading {
  return chrome_test_util::IsLoading();
}

+ (void)startReloading {
  [BrowserCommandDispatcherForMainBVC() reload];
}

#pragma mark - Tab Utilities (EG2)

+ (void)selectTabAtIndex:(NSUInteger)index {
  chrome_test_util::SelectTabAtIndexInCurrentMode(index);
}

+ (BOOL)isIncognitoMode {
  return chrome_test_util::IsIncognitoMode();
}

+ (void)closeTabAtIndex:(NSUInteger)index {
  chrome_test_util::CloseTabAtIndex(index);
}

+ (NSUInteger)mainTabCount {
  return chrome_test_util::GetMainTabCount();
}

+ (NSUInteger)incognitoTabCount {
  return chrome_test_util::GetIncognitoTabCount();
}

+ (NSUInteger)evictedMainTabCount {
  return chrome_test_util::GetEvictedMainTabCount();
}

+ (void)evictOtherTabModelTabs {
  chrome_test_util::EvictOtherTabModelTabs();
}

+ (NSError*)simulateTabsBackgrounding {
  if (!chrome_test_util::SimulateTabsBackgrounding()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to simulate tab backgrounding.");
  }
  return nil;
}

+ (NSError*)setCurrentTabsToBeColdStartTabs {
  if (!chrome_test_util::SetCurrentTabsToBeColdStartTabs()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to state tabs as cold start tabs");
  }
  return nil;
}

+ (NSError*)resetTabUsageRecorder {
  if (!chrome_test_util::ResetTabUsageRecorder()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to reset the TabUsageRecorder");
  }
  return nil;
}

+ (void)openNewTab {
  chrome_test_util::OpenNewTab();
}

+ (void)closeCurrentTab {
  chrome_test_util::CloseCurrentTab();
}

+ (void)openNewIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
}

+ (void)closeAllTabsInCurrentMode {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

+ (NSError*)closeAllIncognitoTabs {
  bool success = chrome_test_util::CloseAllIncognitoTabs();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Could not close all Incognito tabs");
  }
  return nil;
}

+ (void)closeAllTabs {
  chrome_test_util::CloseAllTabs();
}

+ (void)startGoingBack {
  [BrowserCommandDispatcherForMainBVC() goBack];
}

+ (void)startGoingForward {
  [BrowserCommandDispatcherForMainBVC() goForward];
}

#pragma mark - WebState Utilities (EG2)

+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID {
  bool success = web::test::TapWebViewElementWithIdInIframe(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID));
  if (!success) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:@"Failed to tap element with ID=%@", elementID]);
  }

  return nil;
}

+ (BOOL)tapWebStateElementWithID:(NSString*)elementID error:(NSError*)error {
  return web::test::TapWebViewElementWithId(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID), &error);
}

+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state containing element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)submitWebStateFormWithID:(NSString*)formID {
  bool success = web::test::SubmitWebViewFormWithId(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(formID));

  if (!success) {
    NSString* errorString =
        [NSString stringWithFormat:@"Failed to submit form with ID=%@", formID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (BOOL)webStateContainsText:(NSString*)text {
  return web::test::IsWebViewContainingText(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
}

+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_LOADED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view loaded image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_BLOCKED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view blocked image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (void)setContentSettings:(ContentSetting)setting {
  chrome_test_util::SetContentSettingsBlockPopups(setting);
}

+ (NSError*)signOutAndClearAccounts {
  bool success = chrome_test_util::SignOutAndClearAccounts();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Real accounts couldn't be cleared.");
  }
  return nil;
}

#pragma mark - Sync Utilities (EG2)

+ (void)clearAutofillProfileWithGUID:(NSString*)GUID {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  chrome_test_util::ClearAutofillProfile(utfGUID);
}

+ (void)injectAutofillProfileOnFakeSyncServerWithGUID:(NSString*)GUID
                                  autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(utfGUID, utfFullName);
}

+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  return chrome_test_util::IsAutofillProfilePresent(utfGUID, utfFullName);
}

#pragma mark - Bookmarks Utilities (EG2)

+ (NSError*)waitForBookmarksToFinishinLoading {
  bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return chrome_test_util::BookmarksLoaded();
  });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model did not load");
  }
  return nil;
}

+ (NSError*)clearBookmarks {
  bool success = chrome_test_util::ClearBookmarks();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Not all bookmarks were removed.");
  }
  return nil;
}

+ (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return chrome_test_util::GetNumberOfSyncEntities(type);
}

+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title {
  chrome_test_util::InjectBookmarkOnFakeSyncServer(
      base::SysNSStringToUTF8(URL), base::SysNSStringToUTF8(title));
}

+ (void)addFakeSyncServerTypedURL:(NSString*)URL {
  chrome_test_util::InjectTypedURLOnFakeSyncServer(
      base::SysNSStringToUTF8(URL));
}

+ (void)addHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::AddTypedURLOnClient(GURL(base::SysNSStringToUTF8(URL)));
}

+ (void)deleteHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::DeleteTypedUrlFromClient(
      GURL(base::SysNSStringToUTF8(URL)));
}

+ (BOOL)isTypedURL:(NSString*)spec presentOnClient:(BOOL)expectPresent {
  NSError* error = nil;
  GURL URL(base::SysNSStringToUTF8(spec));
  BOOL success =
      chrome_test_util::IsTypedUrlPresentOnClient(URL, expectPresent, &error);
  return success && !error;
}

+ (void)triggerSyncCycleForType:(syncer::ModelType)type {
  chrome_test_util::TriggerSyncCycle(type);
}

+ (void)deleteAutofillProfileOnFakeSyncServerWithGUID:(NSString*)GUID {
  chrome_test_util::DeleteAutofillProfileOnFakeSyncServer(
      base::SysNSStringToUTF8(GUID));
}

#pragma mark - Sync Utilities (EG2)

+ (void)clearSyncServerData {
  chrome_test_util::ClearSyncServerData();
}

+ (void)startSync {
  chrome_test_util::StartSync();
}

+ (void)stopSync {
  chrome_test_util::StopSync();
}

+ (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    return chrome_test_util::IsSyncInitialized() == isInitialized;
  });
  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Sync must be initialized: %@",
                                   isInitialized ? @"YES" : @"NO"];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSString*)syncCacheGUID {
  return base::SysUTF8ToNSString(chrome_test_util::GetSyncCacheGuid());
}

+ (void)setUpFakeSyncServer {
  chrome_test_util::SetUpFakeSyncServer();
}

+ (void)tearDownFakeSyncServer {
  chrome_test_util::TearDownFakeSyncServer();
}

+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count {
  std::string UTF8Name = base::SysNSStringToUTF8(name);
  NSError* __autoreleasing tempError = nil;
  bool success = chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
      (syncer::ModelType)type, UTF8Name, count, &tempError);
  NSError* error = tempError;

  if (!success and !error) {
    NSString* errorString =
        [NSString stringWithFormat:@"Expected %zu entities of the %d type.",
                                   count, (syncer::ModelType)type];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return error;
}

+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs {
  std::multiset<std::string> multisetSpecs;
  for (NSString* spec in specs) {
    multisetSpecs.insert(base::SysNSStringToUTF8(spec));
  }

  NSError* __autoreleasing tempError = nil;
  bool success =
      chrome_test_util::VerifySessionsOnSyncServer(multisetSpecs, &tempError);
  NSError* error = tempError;
  if (!success && !error) {
    error = testing::NSErrorWithLocalizedDescription(
        @"Error occurred during verification sessions.");
  }
  return error;
}

+ (id)executeJavaScript:(NSString*)javaScript error:(NSError**)outError {
  __block bool handlerCalled = false;
  __block id blockResult = nil;
  __block NSError* blockError = nil;
  [chrome_test_util::GetCurrentWebState()->GetJSInjectionReceiver()
      executeJavaScript:javaScript
      completionHandler:^(id result, NSError* error) {
        handlerCalled = true;
        blockResult = [result copy];
        blockError = [error copy];
      }];

  bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return handlerCalled;
  });

  if (completed) {
    NSError* __autoreleasing autoreleasedError = blockError;
    *outError = autoreleasedError;
  } else {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Did not complete execution of JavaScript: %@",
                         javaScript];
    NSError* __autoreleasing autoreleasedError =
        testing::NSErrorWithLocalizedDescription(errorDescription);
    *outError = autoreleasedError;
  }
  return blockResult;
}

#pragma mark - Accessibility Utilities (EG2)

+ (NSError*)verifyAccessibilityForCurrentScreen {
  NSError* error = nil;
  bool success = chrome_test_util::VerifyAccessibilityForCurrentScreen(error);
  if (!success || error) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Accessibility checks failed! Error: %@", error];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

#pragma mark - Check features (EG2)

+ (BOOL)isSlimNavigationManagerEnabled {
  return base::FeatureList::IsEnabled(web::features::kSlimNavigationManager);
}

+ (BOOL)isBlockNewTabPagePendingLoadEnabled {
  return base::FeatureList::IsEnabled(kBlockNewTabPagePendingLoad);
}

+ (BOOL)isNewOmniboxPopupLayoutEnabled {
  return base::FeatureList::IsEnabled(kNewOmniboxPopupLayout);
}

+ (BOOL)isUMACellularEnabled {
  return base::FeatureList::IsEnabled(kUmaCellular);
}

+ (BOOL)isUKMEnabled {
  return base::FeatureList::IsEnabled(ukm::kUkmFeature);
}
+ (BOOL)isWebPaymentsModifiersEnabled {
  return base::FeatureList::IsEnabled(
      payments::features::kWebPaymentsModifiers);
}

#pragma mark - ScopedBlockPopupsPref

+ (ContentSetting)popupPrefValue {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, NULL);
}

+ (void)setPopupPrefValue:(ContentSetting)value {
  DCHECK(value == CONTENT_SETTING_BLOCK || value == CONTENT_SETTING_ALLOW);
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, value);
}

@end
