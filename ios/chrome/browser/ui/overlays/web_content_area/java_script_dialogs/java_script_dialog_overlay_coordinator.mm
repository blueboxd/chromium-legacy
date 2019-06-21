// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/overlays/overlay_ui_dismissal_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogOverlayCoordinator () <
    JavaScriptDialogOverlayMediatorDelegate>
// Whether the coordinator has been started.
@property(nonatomic, getter=isStarted) BOOL started;
// The alert view controller.
@property(nonatomic) AlertViewController* alertViewController;
// The mediator.
@property(nonatomic) JavaScriptDialogOverlayMediator* mediator;
@end

@implementation JavaScriptDialogOverlayCoordinator

#pragma mark - Accessors

- (void)setMediator:(JavaScriptDialogOverlayMediator*)mediator {
  if (_mediator == mediator)
    return;
  _mediator.delegate = nil;
  _mediator = mediator;
  _mediator.delegate = self;
}

#pragma mark - JavaScriptDialogOverlayMediatorDelegate

- (void)stopDialogForMediator:(JavaScriptDialogOverlayMediator*)mediator {
  DCHECK_EQ(self.mediator, mediator);
  [self stopAnimated:YES];
}

#pragma mark - OverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  // Subclasses implement.
  return NO;
}

- (UIViewController*)viewController {
  return self.alertViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started)
    return;
  self.alertViewController = [[AlertViewController alloc] init];
  self.alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  self.mediator = [self newMediator];
  self.mediator.consumer = self.alertViewController;
  [self.baseViewController presentViewController:self.alertViewController
                                        animated:animated
                                      completion:nil];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  __weak __typeof__(self) weakSelf = self;
  [self.baseViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           __typeof__(self) strongSelf = weakSelf;
                           if (!strongSelf)
                             return;
                           strongSelf.alertViewController = nil;
                           strongSelf.dismissalDelegate
                               ->OverlayUIDidFinishDismissal(weakSelf.request);
                         }];
  self.started = NO;
}

@end

@implementation JavaScriptDialogOverlayCoordinator (Subclassing)

- (JavaScriptDialogOverlayMediator*)newMediator {
  NOTREACHED() << "Subclasses implement.";
  return nil;
}

@end
