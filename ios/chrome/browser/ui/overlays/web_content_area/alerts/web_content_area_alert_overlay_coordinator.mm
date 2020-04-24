// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/web_content_area_alert_overlay_coordinator.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+alert_mediator_creation.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator_util.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_alert_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebContentAreaOverlayCoordinator ()
// Returns an array of all the mediator classes supported for this coordinator.
@property(class, nonatomic, readonly) NSArray<Class>* supportedMediatorClasses;
@end

@implementation WebContentAreaOverlayCoordinator

#pragma mark - Accessors

+ (NSArray<Class>*)supportedMediatorClasses {
  static NSArray<Class>* _supportedMediatorClasses = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    _supportedMediatorClasses = @[
      [AlertOverlayMediator class],
      [JavaScriptConfirmationOverlayMediator class],
      [JavaScriptAlertOverlayMediator class],
      [JavaScriptPromptOverlayMediator class],
    ];
  });
  return _supportedMediatorClasses;
}

#pragma mark - OverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  static std::unique_ptr<const OverlayRequestSupport> _requestSupport;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    _requestSupport =
        CreateAggregateSupportForMediators(self.supportedMediatorClasses);
  });
  return _requestSupport.get();
}

@end

@implementation WebContentAreaOverlayCoordinator (AlertMediatorCreation)

- (AlertOverlayMediator*)newMediator {
  AlertOverlayMediator* mediator =
      base::mac::ObjCCast<AlertOverlayMediator>(GetMediatorForRequest(
          [self class].supportedMediatorClasses, self.request));
  DCHECK(mediator) << "None of the supported mediator classes support request.";
  return mediator;
}

@end
