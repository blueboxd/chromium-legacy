// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_camera_controller.h"
#include "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@implementation CreditCardScannerViewController

#pragma mark Lifecycle

- (instancetype)initWithPresentationProvider:
    (id<ScannerPresenting>)presentationProvider {
  self = [super initWithPresentationProvider:presentationProvider];
  return self;
}

#pragma mark - ScannerViewController

- (ScannerView*)buildScannerView {
  return [[CreditCardScannerView alloc] initWithFrame:self.view.frame
                                             delegate:self];
}

- (CameraController*)buildCameraController {
  return [[CreditCardScannerCameraController alloc]
      initWithCreditCardScannerDelegate:self];
}

- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion {
  switch (reason) {
    case scannerViewController::CLOSE_BUTTON:
      base::RecordAction(UserMetricsAction("MobileCreditCardScannerClose"));
      break;
    case scannerViewController::ERROR_DIALOG:
      base::RecordAction(UserMetricsAction("MobileCreditCardScannerError"));
      break;
    case scannerViewController::SCAN_COMPLETE:
      base::RecordAction(
          UserMetricsAction("MobileCreditCardScannerScannedCard"));
      break;
    case scannerViewController::IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE:
      break;
  }

  [super dismissForReason:reason withCompletion:completion];
}

#pragma mark - CreditCardScannerCameraControllerDelegate

- (void)receiveCreditCardScannerResult:(CMSampleBufferRef)sampleBuffer {
  // TODO(crbug.com/986275):Implement method.
}

@end
