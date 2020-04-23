// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_DATA_SOURCE_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

// Fake AlertOverlayMediatorDataSource for use in tests.
@interface FakeAlertOverlayMediatorDataSource
    : NSObject <AlertOverlayMediatorDataSource>

// The text field values to return for the data source protocol.
@property(nonatomic, strong) NSArray<NSString*>* textFieldValues;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_DATA_SOURCE_H_
