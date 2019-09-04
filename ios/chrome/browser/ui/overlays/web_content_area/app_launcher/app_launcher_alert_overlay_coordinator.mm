// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_coordinator.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppLauncherAlertOverlayCoordinator () <
    AppLauncherAlertOverlayMediatorDelegate>
// Whether the coordinator has been started.
@property(nonatomic, getter=isStarted) BOOL started;

@property(nonatomic) AlertViewController* alertViewController;
@property(nonatomic) AppLauncherAlertOverlayMediator* mediator;
@end

@implementation AppLauncherAlertOverlayCoordinator

#pragma mark - Accessors

- (void)setMediator:(AppLauncherAlertOverlayMediator*)mediator {
  if (_mediator == mediator)
    return;
  _mediator.delegate = nil;
  _mediator = mediator;
  _mediator.delegate = self;
}

#pragma mark - AppLauncherAlertOverlayMediatorDelegate

- (void)stopDialogForMediator:(AppLauncherAlertOverlayMediator*)mediator {
  DCHECK_EQ(self.mediator, mediator);
  [self stopAnimated:YES];
}

#pragma mark - OverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
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
  self.mediator =
      [[AppLauncherAlertOverlayMediator alloc] initWithRequest:self.request];
  self.mediator.consumer = self.alertViewController;
  __weak __typeof__(self) weakSelf = self;
  [self.baseViewController
      presentViewController:self.alertViewController
                   animated:animated
                 completion:^{
                   weakSelf.delegate->OverlayUIDidFinishPresentation(
                       weakSelf.request);
                 }];
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
                           strongSelf.delegate->OverlayUIDidFinishDismissal(
                               weakSelf.request);
                         }];
  self.started = NO;
}

@end
