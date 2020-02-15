// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserSigninViewController ()

// View embedded within the user sign-in host.
@property(nonatomic, strong) UIView* embeddedView;
// Activity indicator used to block the UI when a sign-in operation is in
// progress.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;

@end

@implementation UserSigninViewController

#pragma mark - Public

- (void)showEmbeddedViewController:(UIViewController*)viewController {
  DCHECK(viewController);
  DCHECK(!self.embeddedView);

  // Set bounds on |self.embeddedView|.
  self.embeddedView = viewController.view;
  self.embeddedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, self.embeddedView);

  // Add embedded view as a subview to this view controller.
  [self.view addSubview:viewController.view];
  [self addChildViewController:viewController];
  [viewController didMoveToParentViewController:self];

  [self.view setNeedsLayout];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.cr_systemBackgroundColor;

  self.activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  self.activityIndicator.strokeWidth = 3;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];

  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameCenterConstraints(self.view, self.activityIndicator);
}

@end
