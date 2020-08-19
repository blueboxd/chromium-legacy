// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state_metrics/browser_state_metrics.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer.h"
#import "ios/chrome/browser/tabs/tab_parenting_observer.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/public/session/session_certificate_policy_cache.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Updates CRWSessionCertificatePolicyManager's certificate policy cache.
void UpdateCertificatePolicyCacheFromWebState(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    const web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      policy_cache);
}

// Populates the certificate policy cache based on the WebStates of
// |web_state_list|.
void RestoreCertificatePolicyCacheFromModel(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (int index = 0; index < web_state_list->count(); ++index) {
    UpdateCertificatePolicyCacheFromWebState(
        policy_cache, web_state_list->GetWebStateAt(index));
  }
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current entries in |web_state_list|.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK(policy_cache);
  DCHECK(web_state_list);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::Bind(&web::CertificatePolicyCache::ClearCertificatePolicies,
                 policy_cache),
      base::Bind(&RestoreCertificatePolicyCacheFromModel, policy_cache,
                 base::Unretained(web_state_list)));
}

// Records metrics for the interface's orientation.
void RecordInterfaceOrientationMetric() {
  switch ([[UIApplication sharedApplication] statusBarOrientation]) {
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationPortraitUpsideDown:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", YES);
      break;
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", NO);
      break;
    case UIInterfaceOrientationUnknown:
      // TODO(crbug.com/228832): Convert from a boolean histogram to an
      // enumerated histogram and log this case as well.
      break;
  }
}

}  // anonymous namespace

@interface TabModel ()<CRWWebStateObserver, WebStateListObserving> {
  // Weak reference to the underlying shared model implementation.
  WebStateList* _webStateList;

  // Enabler for |_webStateList|
  WebUsageEnablerBrowserAgent* _webEnabler;

  // WebStateListObservers reacting to modifications of the model (may send
  // notification, translate and forward events, update metrics, ...).
  std::vector<std::unique_ptr<WebStateListObserver>> _webStateListObservers;

  // Strong references to id<WebStateListObserving> wrapped by non-owning
  // WebStateListObserverBridges.
  NSArray<id<WebStateListObserving>>* _retainedWebStateListObservers;

  // Weak reference to the session restoration agent.
  SessionRestorationBrowserAgent* _sessionRestorationBrowserAgent;

  // Used for saving gray images.
  SnapshotBrowserAgent* _snapshotBrowserAgent;

  // Used to ensure thread-safety of the certificate policy management code.
  base::CancelableTaskTracker _clearPoliciesTaskTracker;

  // Used to observe owned Tabs' WebStates.
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
}

@end

@implementation TabModel

@synthesize browserState = _browserState;

#pragma mark - Overriden

- (void)dealloc {
  // -disconnect should always have been called before destruction.
  DCHECK(!_browserState);
}

#pragma mark - Public methods

- (NSUInteger)count {
  DCHECK_GE(_webStateList->count(), 0);
  return static_cast<NSUInteger>(_webStateList->count());
}

- (WebStateList*)webStateList {
  DCHECK(_webStateList);
  return _webStateList;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    _webStateList = browser->GetWebStateList();
    _browserState = browser->GetBrowserState();
    DCHECK(_browserState);

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);

    _sessionRestorationBrowserAgent =
        SessionRestorationBrowserAgent::FromBrowser(browser);
    _webEnabler = WebUsageEnablerBrowserAgent::FromBrowser(browser);

    _snapshotBrowserAgent = SnapshotBrowserAgent::FromBrowser(browser);

    NSMutableArray<id<WebStateListObserving>>* retainedWebStateListObservers =
        [[NSMutableArray alloc] init];

    ClosingWebStateObserver* closingWebStateObserver =
        [[ClosingWebStateObserver alloc]
            initWithRestoreService:IOSChromeTabRestoreServiceFactory::
                                       GetForBrowserState(_browserState)];
    [retainedWebStateListObservers addObject:closingWebStateObserver];

    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(self));

    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(closingWebStateObserver));

    _webStateListObservers.push_back(std::make_unique<TabParentingObserver>());

    for (const auto& webStateListObserver : _webStateListObservers)
      _webStateList->AddObserver(webStateListObserver.get());
    _retainedWebStateListObservers = [retainedWebStateListObservers copy];

    // Register for resign active notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(willResignActive:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    // Register for background notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
  }
  return self;
}

// NOTE: This can be called multiple times, so must be robust against that.
- (void)disconnect {
  if (!_browserState)
    return;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  _sessionRestorationBrowserAgent = nullptr;
  _browserState = nullptr;

  // Close all tabs. Do this in an @autoreleasepool as WebStateList observers
  // will be notified (they are unregistered later). As some of them may be
  // implemented in Objective-C and unregister themselves in their -dealloc
  // method, ensure they -autorelease introduced by ARC are processed before
  // the WebStateList destructor is called.
  @autoreleasepool {
    _webStateList->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  }

  // Unregister all observers after closing all the tabs as some of them are
  // required to properly clean up the Tabs.
  for (const auto& webStateListObserver : _webStateListObservers)
    _webStateList->RemoveObserver(webStateListObserver.get());
  _webStateListObservers.clear();
  _retainedWebStateListObservers = nil;
  _webStateList = nullptr;

  _clearPoliciesTaskTracker.TryCancelAll();
  _webStateObserver.reset();
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
// TODO(crbug.com/1115611): Move to SceneController.
- (void)willResignActive:(NSNotification*)notify {
  if (_webEnabler->IsWebUsageEnabled() && _webStateList->GetActiveWebState()) {
    NSString* tabId =
        TabIdTabHelper::FromWebState(_webStateList->GetActiveWebState())
            ->tab_id();

    [_snapshotBrowserAgent->GetSnapshotCache()
        willBeSavedGreyWhenBackgrounding:tabId];
  }
}

// Called when UIApplicationDidEnterBackgroundNotification is received.
// TODO(crbug.com/1115611): Move to SceneController.
- (void)applicationDidEnterBackground:(NSNotification*)notify {
  if (!_browserState)
    return;

  // Evict all the certificate policies except for the current entries of the
  // active sessions.
  CleanCertificatePolicyCache(
      &_clearPoliciesTaskTracker,
      base::CreateSingleThreadTaskRunner({web::WebThread::IO}),
      web::BrowserState::GetCertificatePolicyCache(_browserState),
      _webStateList);

  // Normally, the session is saved after some timer expires but since the app
  // is about to enter the background send YES to save the session immediately.
  _sessionRestorationBrowserAgent->SaveSession(/*immediately=*/true);

  // Write out a grey version of the current website to disk.
  if (_webEnabler->IsWebUsageEnabled() && _webStateList->GetActiveWebState()) {
    NSString* tabId =
        TabIdTabHelper::FromWebState(_webStateList->GetActiveWebState())
            ->tab_id();

    [_snapshotBrowserAgent->GetSnapshotCache()
        saveGreyInBackgroundForSnapshotID:tabId];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (!navigation->HasCommitted())
    return;

  if (!navigation->IsSameDocument() && !self.browserState->IsOffTheRecord()) {
    int tabCount = static_cast<int>(self.count);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tabCount, 1, 200, 50);
  }

  web::NavigationItem* item =
      webState->GetNavigationManager()->GetLastCommittedItem();
  navigation_metrics::RecordMainFrameNavigation(
      item ? item->GetVirtualURL() : GURL::EmptyGURL(),
      navigation->IsSameDocument(), self.browserState->IsOffTheRecord(),
      GetBrowserStateType(webState->GetBrowserState()));
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {

  // In order to avoid false positive in the crash loop detection, disable the
  // counter as soon as an URL is loaded. This requires an user action and is a
  // significant source of crashes. Ignore NTP as it is loaded by default after
  // a crash.
  if (navigation->GetUrl().host_piece() != kChromeUINewTabHost) {
    static dispatch_once_t dispatch_once_token;
    dispatch_once(&dispatch_once_token, ^{
      crash_util::ResetFailedStartupAttemptCount();
    });
  }

  DCHECK(webState->GetNavigationManager());
  web::NavigationItem* navigationItem =
      webState->GetNavigationManager()->GetPendingItem();

  // TODO(crbug.com/676129): the pending item is not correctly set when the
  // page is reloading, use the last committed item if pending item is null.
  // Remove this once tracking bug is fixed.
  if (!navigationItem) {
    navigationItem = webState->GetNavigationManager()->GetLastCommittedItem();
  }

  if (!navigationItem) {
    // Pending item may not exist due to the bug in //ios/web layer.
    // TODO(crbug.com/899827): remove this early return once GetPendingItem()
    // always return valid object inside WebStateObserver::DidStartNavigation()
    // callback.
    //
    // Note that GetLastCommittedItem() returns null if navigation manager does
    // not have committed items (which is normal situation).
    return;
  }

  [[OmniboxGeolocationController sharedInstance]
      addLocationToNavigationItem:navigationItem
                     browserState:ChromeBrowserState::FromBrowserState(
                                      webState->GetBrowserState())];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  RecordInterfaceOrientationMetric();

  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForWebState:webState
                    loadSuccess:success];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  // The TabModel is removed from WebState's observer when the WebState is
  // detached from WebStateList which happens before WebState destructor,
  // so this method should never be called.
  NOTREACHED();
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK(webState);
  webState->AddObserver(_webStateObserver.get());
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK(oldWebState);
  DCHECK(newWebState);
  newWebState->AddObserver(_webStateObserver.get());
  oldWebState->RemoveObserver(_webStateObserver.get());
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  DCHECK(webState);
  webState->RemoveObserver(_webStateObserver.get());
}

@end
