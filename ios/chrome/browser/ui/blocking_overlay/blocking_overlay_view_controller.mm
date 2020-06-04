// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/blocking_overlay/blocking_overlay_view_controller.h"

#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BlockingOverlayViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.view.backgroundColor = UIColor.blackColor;

  UILabel* label = [[UILabel alloc] init];
  // TODO(crbug.com/1072408): use localized text if this goes into final UI.
  label.text = @"Please finish the flow in the other window";
  label.textColor = UIColor.whiteColor;
  [self.view addSubview:label];
  AddSameCenterConstraints(label, self.view);
  label.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button setTitle:@"Show that window" forState:UIControlStateNormal];
  button.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
  button.layer.cornerRadius = 5;
  [button addTarget:self
                action:@selector(buttonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:button];
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor constraintEqualToAnchor:label.leadingAnchor],
    [button.trailingAnchor constraintEqualToAnchor:label.trailingAnchor],
    [button.topAnchor constraintEqualToAnchor:label.bottomAnchor],
  ]];
}

- (void)buttonPressed:(id)sender {
  if (@available(iOS 13, *)) {
    [self.blockingSceneCommandHandler
        activateBlockingScene:self.view.window.windowScene];
  }
}

@end
