// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_coordinator.h"

#import "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardScannerCoordinator () <ScannerPresenting>

// The view controller attached to this coordinator.
@property(nonatomic, strong) UIViewController* viewController;

@end

@implementation CreditCardScannerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  CreditCardScannerViewController* creditCardScannerViewController =
      [[CreditCardScannerViewController alloc]
          initWithPresentationProvider:self];

  creditCardScannerViewController.modalPresentationStyle =
      UIModalPresentationFullScreen;
  self.viewController = creditCardScannerViewController;
  [self.baseViewController presentViewController:creditCardScannerViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [self.viewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;
}

#pragma mark - ScannerPresenting

- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(void (^)(void))completion {
  [self stop];
}

@end
