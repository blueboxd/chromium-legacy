// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_coordinator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/save_password_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBannerOverlayCoordinator ()
// The list of supported mediator classes.
@property(class, nonatomic, readonly) NSArray<Class>* supportedMediatorClasses;
// The banner view being managed by this coordinator.
@property(nonatomic) InfobarBannerViewController* bannerViewController;
@end

@implementation InfobarBannerOverlayCoordinator

#pragma mark - Accessors

+ (NSArray<Class>*)supportedMediatorClasses {
  return @ [[SavePasswordInfobarBannerOverlayMediator class]];
}

+ (const OverlayRequestSupport*)requestSupport {
  static std::unique_ptr<const OverlayRequestSupport> _requestSupport;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    _requestSupport =
        CreateAggregateSupportForMediators(self.supportedMediatorClasses);
  });
  return _requestSupport.get();
}

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request)
    return;
  // Create the mediator and use it aas the delegate for the banner view.
  InfobarOverlayRequestConfig* config =
      self.request->GetConfig<InfobarOverlayRequestConfig>();
  InfobarBannerOverlayMediator* mediator = [self newMediator];
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:mediator
         presentsModal:config->has_badge()
                  type:config->infobar_type()];
  mediator.consumer = self.bannerViewController;
  self.mediator = mediator;
  // Present the banner.
  // TODO(crbug.com/1030357): Use custom presentation.
  self.bannerViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.bannerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  [self.baseViewController presentViewController:self.viewController
                                        animated:animated
                                      completion:^{
                                        [self finishPresentation];
                                      }];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:^{
                                                [self finishDismissal];
                                              }];
  self.started = NO;
}

- (UIViewController*)viewController {
  return self.bannerViewController;
}

#pragma mark - Private

// Called when the presentation of the banner UI is completed.
- (void)finishPresentation {
  // Notify the presentation context that the presentation has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishPresentation(self.request);
  UpdateBannerAccessibilityForPresentation(self.baseViewController,
                                           self.viewController.view);
}

// Called when the dismissal of the banner UI is finished.
- (void)finishDismissal {
  self.bannerViewController = nil;
  self.mediator = nil;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishDismissal(self.request);
  UpdateBannerAccessibilityForDismissal(self.baseViewController);
}

// Creates a mediator instance from the supported mediator class list that
// supports the coordinator's request.
- (InfobarBannerOverlayMediator*)newMediator {
  for (Class mediatorClass in [self class].supportedMediatorClasses) {
    if (mediatorClass.requestSupport->IsRequestSupported(self.request))
      return [[mediatorClass alloc] initWithRequest:self.request];
  }
  NOTREACHED() << "None of the supported mediator classes support request.";
  return nil;
}

@end
