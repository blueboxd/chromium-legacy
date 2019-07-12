// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include "ppapi/cpp/point.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace draw_utils {

TEST(CoordinateTest, GetScreenRect) {
  pp::Rect screen_rect;
  const pp::Rect rect(10, 20, 200, 300);

  // Test various zooms with the position at the origin.
  screen_rect = GetScreenRect(rect, {0, 0}, 1);
  EXPECT_EQ(10, screen_rect.x());
  EXPECT_EQ(20, screen_rect.y());
  EXPECT_EQ(200, screen_rect.width());
  EXPECT_EQ(300, screen_rect.height());

  screen_rect = GetScreenRect(rect, {0, 0}, 1.5);
  EXPECT_EQ(15, screen_rect.x());
  EXPECT_EQ(30, screen_rect.y());
  EXPECT_EQ(300, screen_rect.width());
  EXPECT_EQ(450, screen_rect.height());

  screen_rect = GetScreenRect(rect, {0, 0}, 0.5);
  EXPECT_EQ(5, screen_rect.x());
  EXPECT_EQ(10, screen_rect.y());
  EXPECT_EQ(100, screen_rect.width());
  EXPECT_EQ(150, screen_rect.height());

  // Test various zooms with the position elsewhere.
  screen_rect = GetScreenRect(rect, {400, 30}, 1);
  EXPECT_EQ(-390, screen_rect.x());
  EXPECT_EQ(-10, screen_rect.y());
  EXPECT_EQ(200, screen_rect.width());
  EXPECT_EQ(300, screen_rect.height());

  screen_rect = GetScreenRect(rect, {400, 30}, 1.5);
  EXPECT_EQ(-385, screen_rect.x());
  EXPECT_EQ(0, screen_rect.y());
  EXPECT_EQ(300, screen_rect.width());
  EXPECT_EQ(450, screen_rect.height());

  screen_rect = GetScreenRect(rect, {400, 30}, 0.5);
  EXPECT_EQ(-395, screen_rect.x());
  EXPECT_EQ(-20, screen_rect.y());
  EXPECT_EQ(100, screen_rect.width());
  EXPECT_EQ(150, screen_rect.height());

  // Test various zooms with a negative position.
  screen_rect = GetScreenRect(rect, {100, -50}, 1);
  EXPECT_EQ(-90, screen_rect.x());
  EXPECT_EQ(70, screen_rect.y());
  EXPECT_EQ(200, screen_rect.width());
  EXPECT_EQ(300, screen_rect.height());

  screen_rect = GetScreenRect(rect, {100, -50}, 1.5);
  EXPECT_EQ(-85, screen_rect.x());
  EXPECT_EQ(80, screen_rect.y());
  EXPECT_EQ(300, screen_rect.width());
  EXPECT_EQ(450, screen_rect.height());

  screen_rect = GetScreenRect(rect, {100, -50}, 0.5);
  EXPECT_EQ(-95, screen_rect.x());
  EXPECT_EQ(60, screen_rect.y());
  EXPECT_EQ(100, screen_rect.width());
  EXPECT_EQ(150, screen_rect.height());

  // Test an empty rect always outputs an empty rect.
  const pp::Rect empty_rect;
  screen_rect = GetScreenRect(empty_rect, {20, 500}, 1);
  EXPECT_EQ(-20, screen_rect.x());
  EXPECT_EQ(-500, screen_rect.y());
  EXPECT_EQ(0, screen_rect.width());
  EXPECT_EQ(0, screen_rect.height());

  screen_rect = GetScreenRect(empty_rect, {20, 500}, 1.5);
  EXPECT_EQ(-20, screen_rect.x());
  EXPECT_EQ(-500, screen_rect.y());
  EXPECT_EQ(0, screen_rect.width());
  EXPECT_EQ(0, screen_rect.height());

  screen_rect = GetScreenRect(empty_rect, {20, 500}, 0.5);
  EXPECT_EQ(-20, screen_rect.x());
  EXPECT_EQ(-500, screen_rect.y());
  EXPECT_EQ(0, screen_rect.width());
  EXPECT_EQ(0, screen_rect.height());
}

}  // namespace draw_utils
}  // namespace chrome_pdf
