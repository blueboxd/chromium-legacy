// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarModalOverlayCoordinator ()
// The navigation controller used to display the modal view.
@property(nonatomic) UINavigationController* modalNavController;
@end

@implementation InfobarModalOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request)
    return;
  [self configureModal];
  self.mediator = self.modalMediator;
  self.modalNavController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  // TODO(crbug.com/1030357): Use custom presentation.
  self.modalNavController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.modalNavController.modalTransitionStyle =
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
  return self.modalNavController;
}

#pragma mark - Private

// Called when the presentation of the modal UI is completed.
- (void)finishPresentation {
  // Notify the presentation context that the presentation has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishPresentation(self.request);
}

// Called when the dismissal of the modal UI is finished.
- (void)finishDismissal {
  [self resetModal];
  self.navigationController = nil;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

@end

@implementation InfobarModalOverlayCoordinator (ModalConfiguration)

- (OverlayRequestMediator*)modalMediator {
  NOTREACHED() << "Subclasses implement.";
  return nullptr;
}

- (UIViewController*)modalViewController {
  NOTREACHED() << "Subclasses implement.";
  return nil;
}

- (void)configureModal {
  NOTREACHED() << "Subclasses implement.";
}

- (void)resetModal {
  NOTREACHED() << "Subclasses implement.";
}

@end
