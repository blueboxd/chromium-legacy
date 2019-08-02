// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppLauncherAlertOverlayMediator ()
@property(nonatomic, readonly) OverlayRequest* request;
@property(nonatomic, readonly) AppLauncherAlertOverlayRequestConfig* config;
@end

@implementation AppLauncherAlertOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
    // Verify that the request is configured for app launcher alerts.
    DCHECK(_request->GetConfig<AppLauncherAlertOverlayRequestConfig>());
  }
  return self;
}

#pragma mark - Accessors

- (AppLauncherAlertOverlayRequestConfig*)config {
  return self.request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
}

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (self.consumer == consumer)
    return;
  _consumer = consumer;

  NSString* message = nil;
  NSString* allowActionTitle = nil;
  NSString* rejectActionTitle = l10n_util::GetNSString(IDS_CANCEL);
  if (self.config->is_repeated_request()) {
    message = l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP);
    allowActionTitle =
        l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW);
  } else {
    message = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
    allowActionTitle =
        l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
  }
  [self.consumer setMessage:message];
  __weak __typeof__(self) weakSelf = self;
  [self.consumer setActions:@[
    [AlertAction actionWithTitle:allowActionTitle
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseAllowingAppLaunch:YES];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:rejectActionTitle
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseAllowingAppLaunch:NO];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
  ]];
}

#pragma mark - Response helpers

// Sets the OverlayResponse. |allowAppLaunch| indicates whether the alert's
// allow button was tapped to allow the navigation to open in another app.
- (void)updateResponseAllowingAppLaunch:(BOOL)allowAppLaunch {
  self.request->set_response(
      OverlayResponse::CreateWithInfo<AppLauncherAlertOverlayResponseInfo>(
          allowAppLaunch));
}

@end
