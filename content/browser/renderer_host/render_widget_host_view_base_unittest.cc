// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "content/browser/renderer_host/display_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "ui/display/display.h"

namespace content {

namespace {

display::Display CreateDisplay(int width,
                               int height,
                               display::Display::Rotation rotation) {
  display::Display display;
  display.set_panel_rotation(rotation);
  display.set_bounds(gfx::Rect(width, height));

  return display;
}

} // anonymous namespace

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForMobile) {
  // Square display (width == height).
  {
    display::Display display =
        CreateDisplay(100, 100, display::Display::ROTATE_0);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 200, display::Display::ROTATE_90);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(0, 0, display::Display::ROTATE_180);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 10000, display::Display::ROTATE_270);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));
  }

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, display::Display::ROTATE_0);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(19999, 20000, display::Display::ROTATE_90);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 100, display::Display::ROTATE_180);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(1, 10000, display::Display::ROTATE_270);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));
  }

  // natural width < natural height.
  {
    display::Display display = CreateDisplay(0, 1, display::Display::ROTATE_0);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(20000, 19999, display::Display::ROTATE_90);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(100, 200, display::Display::ROTATE_180);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 1, display::Display::ROTATE_270);
    EXPECT_EQ(SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY,
              DisplayUtil::GetOrientationTypeForMobile(display));
  }
}

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForDesktop) {
  // On Desktop, the primary orientation is the first computed one so a test
  // similar to OrientationTypeForMobile is not possible.
  // Instead this test will only check one configuration and verify that the
  // method reports two landscape and two portrait orientations with one primary
  // and one secondary for each.

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, display::Display::ROTATE_0);
    ScreenOrientationValues landscape_1 =
        DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(landscape_1 == SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY ||
                landscape_1 == SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY);

    display = CreateDisplay(200, 100, display::Display::ROTATE_180);
    ScreenOrientationValues landscape_2 =
        DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(landscape_2 == SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY ||
                landscape_2 == SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY);

    EXPECT_NE(landscape_1, landscape_2);

    display = CreateDisplay(19999, 20000, display::Display::ROTATE_90);
    ScreenOrientationValues portrait_1 =
        DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(portrait_1 == SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY ||
                portrait_1 == SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY);

    display = CreateDisplay(1, 10000, display::Display::ROTATE_270);
    ScreenOrientationValues portrait_2 =
        DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(portrait_2 == SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY ||
                portrait_2 == SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY);

    EXPECT_NE(portrait_1, portrait_2);

  }
}

} // namespace content
