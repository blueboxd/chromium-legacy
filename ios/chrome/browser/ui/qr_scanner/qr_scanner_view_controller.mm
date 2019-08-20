// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/ui/commands/load_query_commands.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"
#include "ios/chrome/browser/ui/scanner/scanner_alerts.h"
#include "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#include "ios/chrome/browser/ui/scanner/scanner_transitioning_delegate.h"
#include "ios/chrome/browser/ui/scanner/scanner_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface QRScannerViewController () {
  // The scanned result.
  NSString* _result;
  // Whether the scanned result should be immediately loaded.
  BOOL _loadResultImmediately;
  // The transitioning delegate used for presenting and dismissing the QR
  // scanner.
  ScannerTransitioningDelegate* _transitioningDelegate;
}

@property(nonatomic, readwrite, weak) id<LoadQueryCommands> queryLoader;

@end

@implementation QRScannerViewController

#pragma mark Lifecycle

- (instancetype)
    initWithPresentationProvider:(id<ScannerPresenting>)presentationProvider
                     queryLoader:(id<LoadQueryCommands>)queryLoader {
  self = [super
      initWithPresentationProvider:presentationProvider
                  cameraController:[CameraController
                                       cameraControllerWithDelegate:self]];
  if (self) {
    _queryLoader = queryLoader;
  }
  return self;
}

#pragma mark - ScannerViewController

- (ScannerView*)buildScannerView {
  return [[QRScannerView alloc] initWithFrame:self.view.frame delegate:self];
}

- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion {
  switch (reason) {
    case scannerViewController::CLOSE_BUTTON:
      base::RecordAction(UserMetricsAction("MobileQRScannerClose"));
      break;
    case scannerViewController::ERROR_DIALOG:
      base::RecordAction(UserMetricsAction("MobileQRScannerError"));
      break;
    case scannerViewController::SCANNED_CODE:
      base::RecordAction(UserMetricsAction("MobileQRScannerScannedCode"));
      break;
    case scannerViewController::IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE:
      break;
  }

  [super dismissForReason:reason withCompletion:completion];
}

@end
