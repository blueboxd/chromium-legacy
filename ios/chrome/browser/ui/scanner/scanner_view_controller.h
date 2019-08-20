// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/scanner/camera_controller.h"
#include "ios/chrome/browser/ui/scanner/scanner_view.h"

namespace scannerViewController {
// The reason why the ScannerViewController was dismissed. Used for collecting
// metrics.
enum DismissalReason {
  CLOSE_BUTTON,
  ERROR_DIALOG,
  SCANNED_CODE,
  // Not reported. Should be kept last of enum.
  IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE
};
}  // namespace scannerViewController

@protocol ScannerPresenting;

// View controller for a generic scanner. This is an abstract class that creates
// the view and camera controller, required for a QR code or Credit Card
// Scanner.
@interface ScannerViewController
    : UIViewController <CameraControllerDelegate, ScannerViewDelegate>

// Stores the camera controller for the scanner.
@property(nonatomic, readwrite, strong) CameraController* cameraController;

// Stores the presentation provider.
@property(nonatomic, readwrite, weak) id<ScannerPresenting>
    presentationProvider;

- (instancetype)initWithPresentationProvider:
                    (id<ScannerPresenting>)presentationProvider
                            cameraController:(CameraController*)cameraController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns a view controller to be presented based on the camera state. Returns
// |self| if the camera is available or an appropriate UIAlertController if
// there was an error loading the camera.
- (UIViewController*)getViewControllerToPresent;

// Builds the scanner view. Must be overridden in the subclass.
- (ScannerView*)buildScannerView;

// Dismiss scanner. Subclass can override to update metrics.
// implementation.
- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion;

- (void)stopReceivingNotifications;

- (void)setTorchMode:(AVCaptureTorchMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_VIEW_CONTROLLER_H_
