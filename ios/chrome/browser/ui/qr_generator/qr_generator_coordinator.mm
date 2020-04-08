
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface QRGeneratorCoordinator () {
  // URL of a page to generate a QR code for.
  GURL _URL;
}

// To be used to dispatch commands to the browser.
@property(nonatomic, strong) CommandDispatcher* dispatcher;

// Main view controller which will be the parent, or ancestor, of other
// view controllers related to this feature.
@property(nonatomic, strong) UINavigationController* viewController;

// View controller used to display the QR code and actions.
@property(nonatomic, strong)
    QRGeneratorViewController* qrGeneratorViewController;

// Title of a page to generate a QR code for.
@property(nonatomic, copy) NSString* title;

@end

@implementation QRGeneratorCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                       URL:(const GURL&)URL {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _title = title;
    _URL = URL;
  }
  return self;
}

#pragma mark - Chrome Coordinator

- (void)start {
  self.dispatcher = self.browser->GetCommandDispatcher();

  self.qrGeneratorViewController = [[QRGeneratorViewController alloc] init];
  self.qrGeneratorViewController.handler =
      HandlerForProtocol(self.dispatcher, QRGenerationCommands);

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:self.qrGeneratorViewController];
  [self.viewController setModalPresentationStyle:UIModalPresentationFormSheet];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
  [super start];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];

  self.qrGeneratorViewController = nil;
  self.viewController = nil;

  [super stop];
}

@end
