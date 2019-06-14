// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace ui {

// This test ensures that PlatformScreen fetches display.
TEST(X11ScreenOzoneTest, FetchDisplay) {
  constexpr uint32_t kMinNumberOfDisplays = 1;
  X11ScreenOzone platform_screen;

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen.GetAllDisplays();
  EXPECT_GE(all_displays.size(), kMinNumberOfDisplays);
}

}  // namespace ui
