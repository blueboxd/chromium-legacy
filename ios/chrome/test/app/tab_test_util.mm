// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/tab_test_util.h"

#import <Foundation/Foundation.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Returns the tab model for the current mode (incognito or normal).
TabModel* GetCurrentTabModel() {
  return GetMainController().interfaceProvider.currentInterface.tabModel;
}

}  // namespace

BOOL IsIncognitoMode() {
  return GetMainController().interfaceProvider.currentInterface.incognito;
}

void OpenNewTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand command];
    id<ApplicationCommands, BrowserCommands> BVCDispatcher =
        chrome_test_util::DispatcherForActiveBrowserViewController();
    if (BVCDispatcher) {
      [BVCDispatcher openURLInNewTab:command];
      return;
    }
      // The TabGrid is currently presented.
    TabModel* tabModel =
        GetMainController().interfaceProvider.mainInterface.tabModel;
    UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
    [GetMainController().tabSwitcher
        dismissWithNewTabAnimationToModel:tabModel
                        withUrlLoadParams:params
                                  atIndex:NSNotFound];
  }
}

void OpenNewIncognitoTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand incognitoTabCommand];
    id<ApplicationCommands, BrowserCommands> BVCDispatcher =
        chrome_test_util::DispatcherForActiveBrowserViewController();
    if (BVCDispatcher) {
      [BVCDispatcher openURLInNewTab:command];
      return;
    }
      // The TabGrid is currently presented.
    TabModel* tabModel =
        GetMainController().interfaceProvider.incognitoInterface.tabModel;
    UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
    [GetMainController().tabSwitcher
        dismissWithNewTabAnimationToModel:tabModel
                        withUrlLoadParams:params
                                  atIndex:NSNotFound];
  }
}

Tab* GetCurrentTab() {
  TabModel* tab_model = GetCurrentTabModel();
  return tab_model.currentTab;
}

Tab* GetNextTab() {
  TabModel* tabModel = GetCurrentTabModel();
  NSUInteger tabCount = [tabModel count];
  if (tabCount < 2)
    return nil;
  Tab* currentTab = [tabModel currentTab];
  NSUInteger nextTabIndex = [tabModel indexOfTab:currentTab] + 1;
  if (nextTabIndex >= tabCount)
    nextTabIndex = 0;
  return [tabModel tabAtIndex:nextTabIndex];
}

Tab* GetTabAtIndexInCurrentMode(NSUInteger index) {
  TabModel* tabModel = GetCurrentTabModel();
  NSUInteger tabCount = [tabModel count];
  if (tabCount < index + 1)
    return nil;
  return [tabModel tabAtIndex:index];
}

void CloseCurrentTab() {
  TabModel* tab_model = GetCurrentTabModel();
  [tab_model closeTab:tab_model.currentTab];
}

void CloseTabAtIndex(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    [GetCurrentTabModel() closeTabAtIndex:index];
  }
}

void CloseAllTabsInCurrentMode() {
  [GetCurrentTabModel() closeAllTabs];
}

void CloseAllTabs() {
  if (GetIncognitoTabCount()) {
    [GetMainController()
            .interfaceProvider.incognitoInterface.tabModel closeAllTabs];
  }
  if (GetMainTabCount()) {
    [GetMainController().interfaceProvider.mainInterface.tabModel closeAllTabs];
  }
}

void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    TabModel* tab_model = GetCurrentTabModel();
    [tab_model setCurrentTab:[tab_model tabAtIndex:index]];
  }
}

NSUInteger GetMainTabCount() {
  return GetMainController().interfaceProvider.mainInterface.tabModel.count;
}

NSUInteger GetIncognitoTabCount() {
  return GetMainController()
      .interfaceProvider.incognitoInterface.tabModel.count;
}

BOOL ResetTabUsageRecorder() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  GetCurrentTabModel().tabUsageRecorder->ResetAll();
  return YES;
}

BOOL SetCurrentTabsToBeColdStartTabs() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  TabModel* tab_model = GetCurrentTabModel();
  WebStateList* web_state_list = tab_model.webStateList;

  std::vector<web::WebState*> web_states;
  web_states.reserve(web_state_list->count());
  for (int index = 0; index < web_state_list->count(); ++index) {
    web_states.push_back(web_state_list->GetWebStateAt(index));
  }

  tab_model.tabUsageRecorder->InitialRestoredTabs(
      web_state_list->GetActiveWebState(), web_states);
  return YES;
}

BOOL SimulateTabsBackgrounding() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  GetCurrentTabModel().tabUsageRecorder->AppDidEnterBackground();
  return YES;
}

void EvictOtherTabModelTabs() {
  id<BrowserInterfaceProvider> provider = GetMainController().interfaceProvider;
  ios::ChromeBrowserState* otherBrowserState =
      IsIncognitoMode() ? provider.mainInterface.browserState
                        : provider.incognitoInterface.browserState;
  // Disabling and enabling web usage will evict all web views.
  WebStateListWebUsageEnabler* enabler =
      WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
          otherBrowserState);
  enabler->SetWebUsageEnabled(false);
  enabler->SetWebUsageEnabled(true);
}

BOOL CloseAllIncognitoTabs() {
  MainController* main_controller = GetMainController();
  DCHECK(main_controller);
  TabModel* tabModel =
      GetMainController().interfaceProvider.incognitoInterface.tabModel;
  DCHECK(tabModel);
  [tabModel closeAllTabs];
  return YES;
}

NSUInteger GetEvictedMainTabCount() {
  TabModel* tabModel =
      GetMainController().interfaceProvider.mainInterface.tabModel;
  if (!tabModel.tabUsageRecorder)
    return 0;
  return tabModel.tabUsageRecorder->EvictedTabsMapSize();
}

}  // namespace chrome_test_util
