// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#include "base/feature_list.h"
#import "ios/chrome/browser/ui/activity_services/activity_scenario.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SharingCoordinator () <ActivityServicePositioner,
                                  ActivityServicePresentation,
                                  QRGenerationCommands>

@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

@property(nonatomic, strong) QRGeneratorCoordinator* qrGeneratorCoordinator;

@property(nonatomic, weak) UIView* originView;

@end

@implementation SharingCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                originView:(UIView*)originView {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _originView = originView;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                        scenario:ActivityScenario::TabShareButton];

  self.activityServiceCoordinator.positionProvider = self;
  self.activityServiceCoordinator.presentationProvider = self;
  self.activityServiceCoordinator.scopedHandler = self;

  [self.activityServiceCoordinator start];
}

- (void)stop {
  [self activityServiceDidEndPresenting];
  [self hideQRCode];
  self.originView = nil;
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return self.originView;
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;
}

#pragma mark - QRGenerationCommands

- (void)generateQRCode:(GenerateQRCodeCommand*)command {
  DCHECK(base::FeatureList::IsEnabled(kQRCodeGeneration));
  self.qrGeneratorCoordinator = [[QRGeneratorCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:command.title
                             URL:command.URL
                         handler:self];
  [self.qrGeneratorCoordinator start];
}

- (void)hideQRCode {
  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;
}

@end
