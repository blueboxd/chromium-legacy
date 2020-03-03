// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_REQUEST_COORDINATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_REQUEST_COORDINATOR_FACTORY_H_

#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

// OverlayRequestCoordinatorFactory for OverlayModality::kTesting.
@interface TestOverlayRequestCoordinatorFactory
    : OverlayRequestCoordinatorFactory
// Initializer for a factory that vends OverlayRequestCoordinators for |browser|
// at OverlayModality::kTesting.
- (instancetype)initWithBrowser:(Browser*)browser;
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_REQUEST_COORDINATOR_FACTORY_H_
