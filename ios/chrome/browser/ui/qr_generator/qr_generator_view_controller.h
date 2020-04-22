// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import <UIKit/UIKit.h>

// View controller that displays a QR code representing a given website.
@interface QRGeneratorViewController : ConfirmationAlertViewController

// URL of the page to generate a QR code for.
@property(nonatomic, copy) NSURL* pageURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
