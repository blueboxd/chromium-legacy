// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include "ppapi/cpp/point.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace draw_utils {

namespace {

constexpr int kBottomSeparator = 4;
constexpr int kHorizontalSeparator = 1;
constexpr PageInsetSizes kLeftInsets{5, 3, 1, 7};
constexpr PageInsetSizes kRightInsets{1, 3, 5, 7};
constexpr PageInsetSizes kSingleViewInsets{5, 3, 5, 7};

void CompareInsetSizes(const PageInsetSizes& expected_insets,
                       const PageInsetSizes& given_insets) {
  EXPECT_EQ(expected_insets.left, given_insets.left);
  EXPECT_EQ(expected_insets.top, given_insets.top);
  EXPECT_EQ(expected_insets.right, given_insets.right);
  EXPECT_EQ(expected_insets.bottom, given_insets.bottom);
}

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

TEST(CoordinateTest, GetPageInsetsForTwoUpView) {
  // Page is on the left side and isn't the last page in the document.
  CompareInsetSizes(kLeftInsets,
                    GetPageInsetsForTwoUpView(0, 10, kSingleViewInsets,
                                              kHorizontalSeparator));

  // Page is on the left side and is the last page in the document.
  CompareInsetSizes(kSingleViewInsets,
                    GetPageInsetsForTwoUpView(10, 11, kSingleViewInsets,
                                              kHorizontalSeparator));

  // Only one page in the document.
  CompareInsetSizes(
      kSingleViewInsets,
      GetPageInsetsForTwoUpView(0, 1, kSingleViewInsets, kHorizontalSeparator));

  // Page is on the right side of the document.
  CompareInsetSizes(
      kRightInsets,
      GetPageInsetsForTwoUpView(1, 4, kSingleViewInsets, kHorizontalSeparator));
}

TEST(CoordinateTest, GetLeftFillRect) {
  // Testing various rectangles with different positions and sizes.
  pp::Rect page_rect(10, 20, 400, 500);
  page_rect = GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(0, page_rect.x());
  EXPECT_EQ(17, page_rect.y());
  EXPECT_EQ(5, page_rect.width());
  EXPECT_EQ(514, page_rect.height());

  page_rect.SetRect(200, 300, 400, 350);
  page_rect = GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(0, page_rect.x());
  EXPECT_EQ(297, page_rect.y());
  EXPECT_EQ(195, page_rect.width());
  EXPECT_EQ(364, page_rect.height());

  page_rect.SetRect(800, 650, 20, 15);
  page_rect = GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(0, page_rect.x());
  EXPECT_EQ(647, page_rect.y());
  EXPECT_EQ(795, page_rect.width());
  EXPECT_EQ(29, page_rect.height());

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  page_rect = GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(0, page_rect.x());
  EXPECT_EQ(-203, page_rect.y());
  EXPECT_EQ(45, page_rect.width());
  EXPECT_EQ(314, page_rect.height());
}

TEST(CoordinateTest, GetRightFillRect) {
  constexpr int kDocWidth = 1000;

  // Testing various rectangles with different positions, sizes, and document
  // widths.
  pp::Rect page_rect(10, 20, 400, 500);
  page_rect = GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator);
  EXPECT_EQ(415, page_rect.x());
  EXPECT_EQ(17, page_rect.y());
  EXPECT_EQ(585, page_rect.width());
  EXPECT_EQ(514, page_rect.height());

  page_rect.SetRect(200, 300, 400, 350);
  page_rect = GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator);
  EXPECT_EQ(605, page_rect.x());
  EXPECT_EQ(297, page_rect.y());
  EXPECT_EQ(395, page_rect.width());
  EXPECT_EQ(364, page_rect.height());

  page_rect.SetRect(200, 300, 400, 350);
  page_rect =
      GetRightFillRect(page_rect, kSingleViewInsets, 800, kBottomSeparator);
  EXPECT_EQ(605, page_rect.x());
  EXPECT_EQ(297, page_rect.y());
  EXPECT_EQ(195, page_rect.width());
  EXPECT_EQ(364, page_rect.height());

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  page_rect = GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator);
  EXPECT_EQ(155, page_rect.x());
  EXPECT_EQ(-203, page_rect.y());
  EXPECT_EQ(845, page_rect.width());
  EXPECT_EQ(314, page_rect.height());
}

TEST(CoordinateTest, GetBottomFillRect) {
  // Testing various rectangles with different positions and sizes.
  pp::Rect page_rect(10, 20, 400, 500);
  page_rect = GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(5, page_rect.x());
  EXPECT_EQ(527, page_rect.y());
  EXPECT_EQ(410, page_rect.width());
  EXPECT_EQ(4, page_rect.height());
  page_rect.SetRect(200, 300, 400, 350);
  page_rect = GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(195, page_rect.x());
  EXPECT_EQ(657, page_rect.y());
  EXPECT_EQ(410, page_rect.width());
  EXPECT_EQ(4, page_rect.height());

  page_rect.SetRect(800, 650, 20, 15);
  page_rect = GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(795, page_rect.x());
  EXPECT_EQ(672, page_rect.y());
  EXPECT_EQ(30, page_rect.width());
  EXPECT_EQ(4, page_rect.height());

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  page_rect = GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator);
  EXPECT_EQ(45, page_rect.x());
  EXPECT_EQ(107, page_rect.y());
  EXPECT_EQ(110, page_rect.width());
  EXPECT_EQ(4, page_rect.height());
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

TEST(CoordinateTest, GetSurroundingRect) {
  constexpr int kDocWidth = 1000;

  // Test various position, sizes, and document width.
  pp::Rect rect = GetSurroundingRect(100, 300, kSingleViewInsets, kDocWidth,
                                     kBottomSeparator);
  EXPECT_EQ(0, rect.x());
  EXPECT_EQ(97, rect.y());
  EXPECT_EQ(1000, rect.width());
  EXPECT_EQ(314, rect.height());

  rect = GetSurroundingRect(40, 200, kSingleViewInsets, kDocWidth,
                            kBottomSeparator);
  EXPECT_EQ(0, rect.x());
  EXPECT_EQ(37, rect.y());
  EXPECT_EQ(1000, rect.width());
  EXPECT_EQ(214, rect.height());

  rect = GetSurroundingRect(200, 500, kSingleViewInsets, kDocWidth,
                            kBottomSeparator);
  EXPECT_EQ(0, rect.x());
  EXPECT_EQ(197, rect.y());
  EXPECT_EQ(1000, rect.width());
  EXPECT_EQ(514, rect.height());

  rect =
      GetSurroundingRect(-100, 300, kSingleViewInsets, 200, kBottomSeparator);
  EXPECT_EQ(0, rect.x());
  EXPECT_EQ(-103, rect.y());
  EXPECT_EQ(200, rect.width());
  EXPECT_EQ(314, rect.height());
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
