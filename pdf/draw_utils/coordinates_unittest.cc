// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include "ppapi/cpp/point.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace draw_utils {

namespace {

constexpr PageInsetSizes kLeftInsets{5, 1, 3, 7};
constexpr PageInsetSizes kRightInsets{1, 5, 3, 7};

}  // namespace

TEST(CoordinateTest, ExpandDocumentSize) {
  pp::Size doc_size(100, 400);

  // Test various expansion sizes.
  pp::Size rect_size(100, 200);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(100, doc_size.width());
  EXPECT_EQ(600, doc_size.height());

  rect_size.SetSize(200, 150);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(200, doc_size.width());
  EXPECT_EQ(750, doc_size.height());

  rect_size.SetSize(100, 300);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(200, doc_size.width());
  EXPECT_EQ(1050, doc_size.height());

  rect_size.SetSize(250, 400);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(250, doc_size.width());
  EXPECT_EQ(1450, doc_size.height());
}

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

TEST(CoordinateTest, GetLeftRectForTwoUpView) {
  pp::Rect left_rect;

  left_rect = GetLeftRectForTwoUpView({200, 400}, {300, 100}, kLeftInsets);
  EXPECT_EQ(105, left_rect.x());
  EXPECT_EQ(103, left_rect.y());
  EXPECT_EQ(194, left_rect.width());
  EXPECT_EQ(390, left_rect.height());

  left_rect = GetLeftRectForTwoUpView({300, 400}, {300, 0}, kLeftInsets);
  EXPECT_EQ(5, left_rect.x());
  EXPECT_EQ(3, left_rect.y());
  EXPECT_EQ(294, left_rect.width());
  EXPECT_EQ(390, left_rect.height());

  // Test rect smaller than shadow insets returns empty rect.
  left_rect = GetLeftRectForTwoUpView({5, 5}, {10, 0}, kLeftInsets);
  EXPECT_EQ(10, left_rect.x());
  EXPECT_EQ(3, left_rect.y());
  EXPECT_EQ(0, left_rect.width());
  EXPECT_EQ(0, left_rect.height());

  // Test empty rect gets positioned.
  left_rect = GetLeftRectForTwoUpView({0, 0}, {100, 0}, kLeftInsets);
  EXPECT_EQ(105, left_rect.x());
  EXPECT_EQ(3, left_rect.y());
  EXPECT_EQ(0, left_rect.width());
  EXPECT_EQ(0, left_rect.height());
}

TEST(CoordinateTest, GetRightRectForTwoUpView) {
  pp::Rect right_rect;

  right_rect = GetRightRectForTwoUpView({200, 400}, {300, 100}, kRightInsets);
  EXPECT_EQ(301, right_rect.x());
  EXPECT_EQ(103, right_rect.y());
  EXPECT_EQ(194, right_rect.width());
  EXPECT_EQ(390, right_rect.height());

  right_rect = GetRightRectForTwoUpView({300, 400}, {300, 0}, kRightInsets);
  EXPECT_EQ(301, right_rect.x());
  EXPECT_EQ(3, right_rect.y());
  EXPECT_EQ(294, right_rect.width());
  EXPECT_EQ(390, right_rect.height());

  // Test rect smaller than shadow insets returns empty rect.
  right_rect = GetRightRectForTwoUpView({5, 5}, {10, 0}, kRightInsets);
  EXPECT_EQ(11, right_rect.x());
  EXPECT_EQ(3, right_rect.y());
  EXPECT_EQ(0, right_rect.width());
  EXPECT_EQ(0, right_rect.height());

  // Test empty rect gets positioned.
  right_rect = GetRightRectForTwoUpView({0, 0}, {100, 0}, kRightInsets);
  EXPECT_EQ(101, right_rect.x());
  EXPECT_EQ(3, right_rect.y());
  EXPECT_EQ(0, right_rect.width());
  EXPECT_EQ(0, right_rect.height());
}

TEST(CoordinateTest, TwoUpViewLayout) {
  pp::Rect left_rect;
  pp::Rect right_rect;
  pp::Point position(1066, 0);

  // Test layout when the widest page is on the left.
  left_rect = GetLeftRectForTwoUpView({826, 1066}, position, kLeftInsets);
  EXPECT_EQ(245, left_rect.x());
  EXPECT_EQ(3, left_rect.y());
  EXPECT_EQ(820, left_rect.width());
  EXPECT_EQ(1056, left_rect.height());

  right_rect = GetRightRectForTwoUpView({1066, 826}, position, kRightInsets);
  EXPECT_EQ(1067, right_rect.x());
  EXPECT_EQ(3, right_rect.y());
  EXPECT_EQ(1060, right_rect.width());
  EXPECT_EQ(816, right_rect.height());

  position.set_y(1066);
  left_rect = GetLeftRectForTwoUpView({826, 1066}, position, kLeftInsets);
  EXPECT_EQ(245, left_rect.x());
  EXPECT_EQ(1069, left_rect.y());
  EXPECT_EQ(820, left_rect.width());
  EXPECT_EQ(1056, left_rect.height());

  right_rect = GetRightRectForTwoUpView({826, 900}, position, kRightInsets);
  EXPECT_EQ(1067, right_rect.x());
  EXPECT_EQ(1069, right_rect.y());
  EXPECT_EQ(820, right_rect.width());
  EXPECT_EQ(890, right_rect.height());

  // Test layout when the widest page is on the right.
  position.set_y(0);
  left_rect = GetLeftRectForTwoUpView({1066, 826}, position, kLeftInsets);
  EXPECT_EQ(5, left_rect.x());
  EXPECT_EQ(3, left_rect.y());
  EXPECT_EQ(1060, left_rect.width());
  EXPECT_EQ(816, left_rect.height());

  right_rect = GetRightRectForTwoUpView({826, 1066}, position, kRightInsets);
  EXPECT_EQ(1067, right_rect.x());
  EXPECT_EQ(3, right_rect.y());
  EXPECT_EQ(820, right_rect.width());
  EXPECT_EQ(1056, right_rect.height());

  position.set_y(1066);
  left_rect = GetLeftRectForTwoUpView({826, 900}, position, kLeftInsets);
  EXPECT_EQ(245, left_rect.x());
  EXPECT_EQ(1069, left_rect.y());
  EXPECT_EQ(820, left_rect.width());
  EXPECT_EQ(890, left_rect.height());

  right_rect = GetRightRectForTwoUpView({826, 1066}, position, kRightInsets);
  EXPECT_EQ(1067, right_rect.x());
  EXPECT_EQ(1069, right_rect.y());
  EXPECT_EQ(820, right_rect.width());
  EXPECT_EQ(1056, right_rect.height());
}

}  // namespace draw_utils
}  // namespace chrome_pdf
