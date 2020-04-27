// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The histogram key to report the latency between the start of the Share Page
// operation and when the UI is ready to be presented.
const char kSharePageLatencyHistogram[] = "IOS.SharePageLatency";
}  // namespace

@interface ActivityServiceCoordinator () <ActivityServicePresentation>

@property(nonatomic, weak)
    id<ActivityServiceCommands, BrowserCommands, SnackbarCommands>
        handler;

// The time when the Share Page operation started.
@property(nonatomic, assign) base::TimeTicks sharePageStartTime;

@end

@implementation ActivityServiceCoordinator

#pragma mark - Public methods

- (void)start {
  self.handler = static_cast<
      id<ActivityServiceCommands, BrowserCommands, SnackbarCommands>>(
      self.browser->GetCommandDispatcher());

  self.sharePageStartTime = base::TimeTicks::Now();
  __weak __typeof(self) weakSelf = self;
  activity_services::RetrieveCanonicalUrl(
      self.browser->GetWebStateList()->GetActiveWebState(), ^(const GURL& url) {
        [weakSelf sharePageWithCanonicalURL:url];
      });
}

- (void)stop {
  [[ActivityServiceController sharedInstance] cancelShareAnimated:NO];
}

#pragma mark - ActivityServicePresentation

- (void)presentActivityServiceViewController:(UIViewController*)controller {
  [self.baseViewController presentViewController:controller
                                        animated:YES
                                      completion:nil];
}

- (void)activityServiceDidEndPresenting {
  [self.handler hideActivityView];
}

#pragma mark - Private Methods

// Shares the current page using the |canonicalURL|.
- (void)sharePageWithCanonicalURL:(const GURL&)canonicalURL {
  ShareToData* data = activity_services::ShareToDataForWebState(
      self.browser->GetWebStateList()->GetActiveWebState(), canonicalURL);
  if (!data)
    return;

  ActivityServiceController* controller =
      [ActivityServiceController sharedInstance];
  if ([controller isActive])
    return;

  if (self.sharePageStartTime != base::TimeTicks()) {
    UMA_HISTOGRAM_TIMES(kSharePageLatencyHistogram,
                        base::TimeTicks::Now() - self.sharePageStartTime);
    self.sharePageStartTime = base::TimeTicks();
  }

  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  [controller shareWithData:data
               browserState:self.browser->GetBrowserState()
                 dispatcher:self.handler
           positionProvider:self.positionProvider
       presentationProvider:self];
}

@end
