// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

@class AlertAction;
@class TextFieldConfiguration;

// Fake subclass of AlertOverlayMediator for tests.
@interface FakeAlertOverlayMediator : AlertOverlayMediator
// Define readwrite versions of subclassing properties.
@property(nonatomic, readwrite) NSString* alertTitle;
@property(nonatomic, readwrite) NSString* alertMessage;
@property(nonatomic, readwrite)
    NSArray<TextFieldConfiguration*>* alertTextFieldConfigurations;
@property(nonatomic, readwrite) NSArray<AlertAction*>* alertActions;
@property(nonatomic, readwrite) NSString* alertAccessibilityIdentifier;
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_FAKE_ALERT_OVERLAY_MEDIATOR_H_
