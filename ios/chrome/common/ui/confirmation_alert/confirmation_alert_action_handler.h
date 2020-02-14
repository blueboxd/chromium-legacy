// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol ConfirmationAlertActionHandler <NSObject>

// The confirmation should be dismissed.
- (void)confirmationAlertDone;

// The "Primary Action" was touched.
- (void)confirmationAlertPrimaryAction;

// The "Learn More" button was touched.
- (void)confirmationAlertLearnMoreAction;

@end

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_
