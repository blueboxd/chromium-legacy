// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_legacy_coordinator.h"

#import <Foundation/Foundation.h>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/fullscreen/chrome_coordinator+fullscreen_disabling.h"
#import "ios/chrome/browser/ui/page_info/legacy_page_info_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoLegacyCoordinator ()

// The view controller for the Page Info UI. Nil if not visible.
@property(nonatomic, strong)
    LegacyPageInfoViewController* pageInfoViewController;

@end

@implementation PageInfoLegacyCoordinator

@synthesize presentationProvider = _presentationProvider;

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  web::NavigationItem* navItem =
      webState->GetNavigationManager()->GetVisibleItem();

  // It is fully expected to have a navItem here, as showPageInfoPopup can only
  // be trigerred by a button enabled when a current item matches some
  // conditions. However a crash was seen were navItem was NULL hence this
  // test after a DCHECK.
  DCHECK(navItem);
  if (!navItem)
    return;

  // Don't show the bubble twice (this can happen when tapping very quickly in
  // accessibility mode).
  if (self.pageInfoViewController)
    return;

  base::RecordAction(base::UserMetricsAction("MobileToolbarPageSecurityInfo"));

  [self.presentationProvider prepareForPageInfoPresentation];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kPageInfoWillShowNotification
                    object:nil];

  // Disable fullscreen while the page info UI is displayed.
  [self didStartFullscreenDisablingUI];

  GURL url = navItem->GetURL();
  bool presentingOfflinePage =
      OfflinePageTabHelper::FromWebState(webState)->presenting_offline_page();

  PageInfoSiteSecurityDescription* config =
      [PageInfoSiteSecurityMediator configurationForURL:navItem->GetURL()
                                              SSLStatus:navItem->GetSSL()
                                            offlinePage:presentingOfflinePage];

  CGPoint originPresentationCoordinates = [self.presentationProvider
      convertToPresentationCoordinatesForOrigin:self.originPoint];
  self.pageInfoViewController = [[LegacyPageInfoViewController alloc]
             initWithModel:config
               sourcePoint:originPresentationCoordinates
      presentationProvider:self.presentationProvider
                   handler:HandlerForProtocol(
                               self.browser->GetCommandDispatcher(),
                               BrowserCommands)];
}

- (void)stop {
  // Early return if the PageInfoPopup is not presented.
  if (!self.pageInfoViewController)
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kPageInfoWillHideNotification
                    object:nil];

  // Stop disabling fullscreen since the page info UI was stopped.
  [self didStopFullscreenDisablingUI];

  [self.pageInfoViewController dismiss];
  self.pageInfoViewController = nil;
}

@end
