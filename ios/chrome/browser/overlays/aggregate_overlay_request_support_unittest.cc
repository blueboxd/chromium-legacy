// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/aggregate_overlay_request_support.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"

namespace {
// Fake request config types.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FirstConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SecondConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ThirdConfig);
}  // namespace

using AggregateOverlayRequestSupportTest = PlatformTest;

// Tests that support is correctly aggregated.
TEST_F(AggregateOverlayRequestSupportTest, AggregateSupport) {
  // Create an AggregateOverlayRequestSupport that supports requests for
  // FirstConfig and SecondConfig.
  const std::vector<const OverlayRequestSupport*> support_list = {
      FirstConfig::RequestSupport(), SecondConfig::RequestSupport()};
  const AggregateOverlayRequestSupport aggregate_support(support_list);

  // Verify that requests created with FirstConfig and SecondConfig are
  // supported.
  EXPECT_TRUE(aggregate_support.IsRequestSupported(
      OverlayRequest::CreateWithConfig<FirstConfig>().get()));
  EXPECT_TRUE(aggregate_support.IsRequestSupported(
      OverlayRequest::CreateWithConfig<SecondConfig>().get()));

  // Verify that requests created with ThirdConfig are not supported.
  EXPECT_FALSE(aggregate_support.IsRequestSupported(
      OverlayRequest::CreateWithConfig<ThirdConfig>().get()));
}
