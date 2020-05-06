// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_browser_agent.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"

#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Internal implementation of BrowserInterface -- for the most part a wrapper
// around BrowserCoordinator.
@interface WrangledBrowser : NSObject <BrowserInterface>

@property(nonatomic, weak, readonly) BrowserCoordinator* coordinator;

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator;

@end

@implementation WrangledBrowser

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator {
  if (self = [super init]) {
    DCHECK(coordinator.browser);
    _coordinator = coordinator;
  }
  return self;
}

- (UIViewController*)viewController {
  return self.coordinator.viewController;
}

- (BrowserViewController*)bvc {
  return self.coordinator.viewController;
}

- (TabModel*)tabModel {
  return self.browser->GetTabModel();
}

- (Browser*)browser {
  return self.coordinator.browser;
}

- (ChromeBrowserState*)browserState {
  return self.browser->GetBrowserState();
}

- (BOOL)userInteractionEnabled {
  return self.coordinator.active;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  self.coordinator.active = userInteractionEnabled;
}

- (BOOL)incognito {
  return self.browserState->IsOffTheRecord();
}

- (void)setPrimary:(BOOL)primary {
  [self.coordinator.viewController setPrimary:primary];
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.coordinator clearPresentedStateWithCompletion:completion
                                       dismissOmnibox:dismissOmnibox];
}

@end

@interface BrowserViewWrangler () {
  ChromeBrowserState* _browserState;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  __weak id<BrowsingDataCommands> _browsingDataCommandEndpoint;
  BOOL _isShutdown;

  std::unique_ptr<Browser> _mainBrowser;
  std::unique_ptr<Browser> _otrBrowser;
}

@property(nonatomic, strong, readwrite) WrangledBrowser* mainInterface;
@property(nonatomic, strong, readwrite) WrangledBrowser* incognitoInterface;

// Backing objects.
@property(nonatomic) BrowserCoordinator* mainBrowserCoordinator;
@property(nonatomic) BrowserCoordinator* incognitoBrowserCoordinator;
//@property(nonatomic, readonly) TabModel* mainTabModel;
//@property(nonatomic, readonly) TabModel* otrTabModel;
@property(nonatomic, readonly) Browser* mainBrowser;
@property(nonatomic, readonly) Browser* otrBrowser;

// Setters for the main and otr Browsers.
- (void)setMainBrowser:(std::unique_ptr<Browser>)browser;
- (void)setOtrBrowser:(std::unique_ptr<Browser>)browser;

// Creates a new off-the-record ("incognito") browser state for |_browserState|,
// then creates and sets up a TabModel and returns a Browser for the result.
- (std::unique_ptr<Browser>)buildOtrBrowser:(BOOL)restorePersistedState;

// Creates the correct BrowserCoordinator for the corresponding browser state
// and Browser.
- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser;
@end

@implementation BrowserViewWrangler

@synthesize currentInterface = _currentInterface;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
         browsingDataCommandEndpoint:
             (id<BrowsingDataCommands>)browsingDataCommandEndpoint {
  if ((self = [super init])) {
    _browserState = browserState;
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _browsingDataCommandEndpoint = browsingDataCommandEndpoint;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

- (void)createMainBrowser {
  _mainBrowser = Browser::Create(_browserState);
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(_mainBrowser->GetBrowserState());
  browserList->AddBrowser(_mainBrowser.get());
  [self dispatchToEndpointsForBrowser:_mainBrowser.get()];

  SessionRestorationBrowserAgent::FromBrowser(_mainBrowser.get())
      ->SetSessionID(base::SysNSStringToUTF8(_sessionID));
  SessionRestorationBrowserAgent::FromBrowser(_mainBrowser.get())
      ->RestoreSession();
  breakpad::MonitorTabStateForWebStateList(_mainBrowser->GetWebStateList());
  // Follow loaded URLs in the main tab model to send those in case of
  // crashes.
  breakpad::MonitorURLsForWebStateList(self.mainBrowser->GetWebStateList());

  // Create the main coordinator, and thus the main interface.
  _mainBrowserCoordinator = [self coordinatorForBrowser:self.mainBrowser];
  [_mainBrowserCoordinator start];
  DCHECK(_mainBrowserCoordinator.viewController);
  _mainInterface =
      [[WrangledBrowser alloc] initWithCoordinator:_mainBrowserCoordinator];
}

#pragma mark - BrowserViewInformation property implementations

- (void)setCurrentInterface:(WrangledBrowser*)interface {
  DCHECK(interface);
  // |interface| must be one of the interfaces this class already owns.
  DCHECK(self.mainInterface == interface ||
         self.incognitoInterface == interface);
  if (self.currentInterface == interface) {
    return;
  }

  if (self.currentInterface) {
    // Tell the current BVC it moved to the background.
    [self.currentInterface setPrimary:NO];

    // Data storage for the browser is always owned by the current BVC, so it
    // must be updated when switching between BVCs.
    [self changeStorageFromBrowserState:self.currentInterface.browserState
                         toBrowserState:interface.browserState];
  }

  _currentInterface = interface;

  // Update the shared active URL for the new interface.
  DeviceSharingBrowserAgent::FromBrowser(_currentInterface.browser)
      ->UpdateForActiveBrowser();
}

- (id<BrowserInterface>)incognitoInterface {
  if (!_mainInterface)
    return nil;
  if (!_incognitoInterface) {
    // The backing coordinator should not have been created yet.
    DCHECK(!_incognitoBrowserCoordinator);
    ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    _incognitoBrowserCoordinator = [self coordinatorForBrowser:self.otrBrowser];
    [_incognitoBrowserCoordinator start];
    DCHECK(_incognitoBrowserCoordinator.viewController);
    _incognitoInterface = [[WrangledBrowser alloc]
        initWithCoordinator:_incognitoBrowserCoordinator];
  }
  return _incognitoInterface;
}

- (Browser*)mainBrowser {
  DCHECK(_mainBrowser.get())
      << "-createMainBrowser must be called before -mainBrowser is accessed.";
  return _mainBrowser.get();
}

- (Browser*)otrBrowser {
  if (!_otrBrowser) {
    _otrBrowser = [self buildOtrBrowser:YES];
  }
  return _otrBrowser.get();
}

- (void)setMainBrowser:(std::unique_ptr<Browser>)mainBrowser {
  if (_mainBrowser.get()) {
    TabModel* tabModel = self.mainBrowser->GetTabModel();
    WebStateList* webStateList = self.mainBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    breakpad::StopMonitoringURLsForWebStateList(webStateList);
    [tabModel disconnect];
  }

  _mainBrowser = std::move(mainBrowser);
}

- (void)setOtrBrowser:(std::unique_ptr<Browser>)otrBrowser {
  if (_otrBrowser.get()) {
    TabModel* tabModel = self.otrBrowser->GetTabModel();
    WebStateList* webStateList = self.otrBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    [tabModel disconnect];
  }

  _otrBrowser = std::move(otrBrowser);
}

#pragma mark - Mode Switching

- (void)switchGlobalStateToMode:(ApplicationMode)mode {
  // TODO(crbug.com/1048690): use scene-local storage in multiwindow.
  const BOOL incognito = (mode == ApplicationMode::INCOGNITO);
  // Write the state to disk of what is "active".
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:incognito forKey:kIncognitoCurrentKey];
  // Save critical state information for switching between normal and
  // incognito.
  [standardDefaults synchronize];
}

// Updates the local storage, cookie store, and sets the global state.
- (void)changeStorageFromBrowserState:(ChromeBrowserState*)oldState
                       toBrowserState:(ChromeBrowserState*)newState {
  ApplicationMode mode = newState->IsOffTheRecord() ? ApplicationMode::INCOGNITO
                                                    : ApplicationMode::NORMAL;
  [self switchGlobalStateToMode:mode];
}

#pragma mark - Other public methods

- (void)destroyAndRebuildIncognitoBrowser {
  // It is theoretically possible that a Tab has been added to |_otrTabModel|
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(![self.otrBrowser->GetTabModel() count]);
  DCHECK(_browserState);

  // Remove the OTR browser from the browser list. The browser itself is
  // still alive during this call, so any observers can act on it.
  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  browserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Stop watching the OTR webStateList's state for crashes.
  breakpad::StopMonitoringTabStateForWebStateList(
      self.otrBrowser->GetWebStateList());

  // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
  // constructed by calling the property getter.
  BOOL otrBVCIsCurrent = self.currentInterface == self.incognitoInterface;
  @autoreleasepool {
    // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
    // constructed by calling the property getter.
    [_incognitoBrowserCoordinator stop];
    _incognitoBrowserCoordinator = nil;
    _incognitoInterface = nil;

    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    [self setOtrBrowser:nullptr];
    if (otrBVCIsCurrent) {
      _currentInterface = nil;
    }
  }

  _browserState->DestroyOffTheRecordChromeBrowserState();

  // An empty _otrTabModel must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  [self setOtrBrowser:[self buildOtrBrowser:NO]];
  DCHECK(![self.otrBrowser->GetTabModel() count]);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  if (otrBVCIsCurrent) {
    self.currentInterface = self.incognitoInterface;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  // At this stage, new BrowserCoordinators shouldn't be lazily constructed by
  // calling their property getters.
  [_mainBrowserCoordinator stop];
  _mainBrowserCoordinator = nil;
  [_incognitoBrowserCoordinator stop];
  _incognitoBrowserCoordinator = nil;

  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.mainBrowser->GetBrowserState());
  browserList->RemoveBrowser(self.mainBrowser);
  BrowserList* otrBrowserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  otrBrowserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Handles removing observers, stopping breakpad monitoring, and closing all
  // tabs.
  [self setMainBrowser:nullptr];
  [self setOtrBrowser:nullptr];

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (std::unique_ptr<Browser>)buildOtrBrowser:(BOOL)restorePersistedState {
  DCHECK(_browserState);
  // Ensure that the OTR ChromeBrowserState is created.
  ChromeBrowserState* otrBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  std::unique_ptr<Browser> browser = Browser::Create(otrBrowserState);
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browser->GetBrowserState());
  browserList->AddIncognitoBrowser(browser.get());
  [self dispatchToEndpointsForBrowser:browser.get()];
  SessionRestorationBrowserAgent::FromBrowser(browser.get())
      ->SetSessionID(base::SysNSStringToUTF8(_sessionID));
  if (restorePersistedState) {
    SessionRestorationBrowserAgent::FromBrowser(browser.get())
        ->RestoreSession();
  }

  breakpad::MonitorTabStateForWebStateList(browser->GetWebStateList());

  return browser;
}

- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser {
  BrowserCoordinator* coordinator =
      [[BrowserCoordinator alloc] initWithBaseViewController:nil
                                                     browser:browser];
  return coordinator;
}

- (void)dispatchToEndpointsForBrowser:(Browser*)browser {
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:_applicationCommandEndpoint
                   forProtocol:@protocol(ApplicationCommands)];
  // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
  // passed protocol conforms to, so ApplicationSettingsCommands is explicitly
  // dispatched to the endpoint as well. Since this is potentially
  // fragile, DCHECK that it should still work (if the endpoint is non-nil).
  DCHECK(!_applicationCommandEndpoint ||
         [_applicationCommandEndpoint
             conformsToProtocol:@protocol(ApplicationSettingsCommands)]);
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:_applicationCommandEndpoint
                   forProtocol:@protocol(ApplicationSettingsCommands)];
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:_browsingDataCommandEndpoint
                   forProtocol:@protocol(BrowsingDataCommands)];
}

@end
