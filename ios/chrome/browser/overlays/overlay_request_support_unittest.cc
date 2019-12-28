// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"

namespace {
// Fake request config type for use in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FakeConfig);
}  // namespace

using SupportsOverlayRequestTest = PlatformTest;

// Tests that OverlayRequestSupport::All() supports arbitrary config types.
TEST_F(SupportsOverlayRequestTest, SupportAll) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeConfig>();

  const OverlayRequestSupport* support = OverlayRequestSupport::All();
  EXPECT_TRUE(support->IsRequestSupported(first_request.get()));
  EXPECT_TRUE(support->IsRequestSupported(second_request.get()));
}

// Tests that OverlayRequestSupport::None() does not support config types.
TEST_F(SupportsOverlayRequestTest, SupportNone) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeConfig>();

  const OverlayRequestSupport* support = OverlayRequestSupport::None();
  EXPECT_FALSE(support->IsRequestSupported(first_request.get()));
  EXPECT_FALSE(support->IsRequestSupported(second_request.get()));
}

// Tests that the SupportsOverlayRequest template returns true when
// IsRequestSupported() is called with a request with the config type used to
// create the template specialization.
TEST_F(SupportsOverlayRequestTest, SupportsRequestTemplate) {
  std::unique_ptr<OverlayRequestSupport> support =
      std::make_unique<SupportsOverlayRequest<FakeConfig>>();

  // Verify that FakeOverlayUserData requests aren't supported.
  std::unique_ptr<OverlayRequest> unsupported_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  EXPECT_FALSE(support->IsRequestSupported(unsupported_request.get()));

  // Verify that FakeConfig requests are supported.
  std::unique_ptr<OverlayRequest> supported_request =
      OverlayRequest::CreateWithConfig<FakeConfig>();
  EXPECT_TRUE(support->IsRequestSupported(supported_request.get()));
}
