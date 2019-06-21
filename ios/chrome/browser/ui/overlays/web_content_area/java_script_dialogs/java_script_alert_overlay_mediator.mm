// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_alert_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_blocking_action.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptAlertOverlayMediator ()
// The alert config.
@property(nonatomic, readonly) JavaScriptAlertOverlayRequestConfig* config;
@end

@implementation JavaScriptAlertOverlayMediator

#pragma mark - Accessors

- (const JavaScriptDialogSource*)requestSource {
  return &self.config->source();
}

- (JavaScriptAlertOverlayRequestConfig*)config {
  return self.request->GetConfig<JavaScriptAlertOverlayRequestConfig>();
}

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (self.consumer == consumer)
    return;
  [super setConsumer:consumer];
  [self.consumer setMessage:base::SysUTF8ToNSString(self.config->message())];
  __weak __typeof__(self) weakSelf = self;
  NSMutableArray* actions = [@[ [AlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_OK)
                style:UIAlertActionStyleDefault
              handler:^(AlertAction* action) {
                [weakSelf.delegate stopDialogForMediator:weakSelf];
              }] ] mutableCopy];
  AlertAction* blockingAction = GetBlockingAlertAction(self);
  if (blockingAction)
    [actions addObject:blockingAction];
  [self.consumer setActions:actions];
}

@end
