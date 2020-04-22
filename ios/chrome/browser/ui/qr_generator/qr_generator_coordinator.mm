
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface QRGeneratorCoordinator () <ConfirmationAlertActionHandler> {
  // URL of a page to generate a QR code for.
  GURL _URL;
}

// To be used to handle behaviors that go outside the scope of this class.
@property(nonatomic, strong) id<QRGenerationCommands> handler;

// View controller used to display the QR code and actions.
@property(nonatomic, strong) QRGeneratorViewController* viewController;

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
  self.handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                    QRGenerationCommands);

  self.viewController = [[QRGeneratorViewController alloc] init];

  [self.viewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.viewController setPageURL:net::NSURLWithGURL(_URL)];
  [self.viewController setTitleString:self.title];
  [self.viewController setActionHandler:self];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
  [super start];
}

- (void)stop {
  [self.viewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDone {
  [self.handler hideQRCode];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
  // TODO crbug.com/1064990: Add sharing action.
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
  // TODO crbug.com/1064990: Add learn more behavior.
}

@end
