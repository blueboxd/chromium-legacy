// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/qr_scanner/camera_controller.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"

@protocol LoadQueryCommands;
@protocol QRScannerPresenting;

// View controller for the QR scanner. Subclass can inherit
// QRScannerViewDelegate.
@interface QRScannerViewController
    : UIViewController <CameraControllerDelegate, QRScannerViewDelegate>

- (instancetype)initWithPresentationProvider:
                    (id<QRScannerPresenting>)presentationProvider
                                 queryLoader:(id<LoadQueryCommands>)queryLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns a view controller to be presented based on the camera state. Returns
// |self| if the camera is available or an appropriate UIAlertController if
// there was an error loading the camera.
- (UIViewController*)getViewControllerToPresent;

// Builds a scanner view overlay. Can be overridden by a subclass.
- (QRScannerView*)scannerView;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
