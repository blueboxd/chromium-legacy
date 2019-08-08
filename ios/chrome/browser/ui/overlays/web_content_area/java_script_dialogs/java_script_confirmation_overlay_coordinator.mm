// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator+subclassing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

@interface JavaScriptConfirmationOverlayCoordinator ()

// Returns the confirmation configuration from the OverlayRequest.
@property(nonatomic, readonly)
    JavaScriptConfirmationOverlayRequestConfig* confirmationConfig;

@end

@implementation JavaScriptConfirmationOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<JavaScriptConfirmationOverlayRequestConfig>();
}

@end

@implementation JavaScriptConfirmationOverlayCoordinator (Subclassing)

- (JavaScriptDialogOverlayMediator*)newMediator {
  return [[JavaScriptConfirmationOverlayMediator alloc]
      initWithRequest:self.request];
}

@end
