
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface QRGeneratorCoordinator () <ActivityServicePositioner,
                                      ActivityServicePresentation,
                                      ConfirmationAlertActionHandler> {
  // URL of a page to generate a QR code for.
  GURL _URL;
}

// To be used to handle behaviors that go outside the scope of this class.
@property(nonatomic, strong) id<QRGenerationCommands> handler;

// View controller used to display the QR code and actions.
@property(nonatomic, strong) QRGeneratorViewController* viewController;

// Coordinator for the activity view brought up when the user wants to share
// the QR code.
@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

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

  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDone {
  [self.handler hideQRCode];
}

- (void)confirmationAlertPrimaryAction {
  self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  self.activityServiceCoordinator.positionProvider = self;
  self.activityServiceCoordinator.presentationProvider = self;

  // Configure the image sharing scenario.
  self.activityServiceCoordinator.image = self.viewController.content;
  self.activityServiceCoordinator.title = l10n_util::GetNSStringF(
      IDS_IOS_QR_CODE_ACTIVITY_TITLE, base::SysNSStringToUTF16(self.title));

  [self.activityServiceCoordinator start];
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
  // TODO crbug.com/1064990: Add learn more behavior.
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return [self.viewController primaryActionButton];
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;
}

@end
