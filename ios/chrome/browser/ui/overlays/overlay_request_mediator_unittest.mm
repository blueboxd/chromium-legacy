// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for OverlayRequestMediator.
using OverlayRequestMediatorTest = PlatformTest;

// Tests that the mediator's request is reset after destruction.
TEST_F(OverlayRequestMediatorTest, ResetRequestAfterDestruction) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequestMediator* mediator =
      [[OverlayRequestMediator alloc] initWithRequest:request.get()];
  EXPECT_EQ(request.get(), mediator.request);
  request = nullptr;
  EXPECT_EQ(nullptr, mediator.request);
}
