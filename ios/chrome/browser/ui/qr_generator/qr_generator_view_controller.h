// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"

#import <UIKit/UIKit.h>

// View controller that displays a QR code representing a given website.
@interface QRGeneratorViewController : UIViewController

// Command handler for communicating with other components of the app.
@property(nonatomic, weak) id<QRGenerationCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
