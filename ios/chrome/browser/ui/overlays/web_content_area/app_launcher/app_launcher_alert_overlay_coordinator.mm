// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_coordinator.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_mediator.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/presenters/non_modal_view_controller_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppLauncherAlertOverlayCoordinator () <
    AppLauncherAlertOverlayMediatorDelegate,
    ContainedPresenterDelegate>
// Whether the coordinator has been started.
@property(nonatomic, getter=isStarted) BOOL started;

@property(nonatomic) AlertViewController* alertViewController;
@property(nonatomic) NonModalViewControllerPresenter* presenter;
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

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  self.delegate->OverlayUIDidFinishPresentation(self.request);
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.alertViewController = nil;
  self.presenter = nil;
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

#pragma mark - OverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
}

+ (BOOL)usesChildViewController {
  return YES;
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
  self.presenter = [[NonModalViewControllerPresenter alloc] init];
  self.presenter.delegate = self;
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = self.alertViewController;
  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:animated];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  [self.presenter dismissAnimated:animated];
  self.started = NO;
}

@end
