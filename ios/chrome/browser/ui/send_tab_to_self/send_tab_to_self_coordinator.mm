// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SendTabToSelfCoordinator () <UIViewControllerTransitioningDelegate>

// The presentationController that shows the Send Tab To Self UI.
@property(nonatomic, strong) SendTabToSelfModalPresentationController*
    sendTabToSelfModalPresentationController;

@end

@implementation SendTabToSelfCoordinator

#pragma mark - ChromeCoordinator Methods

- (void)start {
  SendTabToSelfTableViewController* tableViewController =
      [[SendTabToSelfTableViewController alloc] init];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:tableViewController];

  navigationController.transitioningDelegate = self;
  navigationController.modalPresentationStyle = UIModalPresentationCustom;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  NOTIMPLEMENTED();
}

#pragma mark-- UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  SendTabToSelfModalPresentationController* presentationController =
      [[SendTabToSelfModalPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  // TODO(crbug.com/970284) flesh out presentationController.
  return presentationController;
}

@end
