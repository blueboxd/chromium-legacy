// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include "pdf/test/test_utils.h"
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

TEST(CoordinateTest, AdjustBottomGapForRightSidePage) {
  pp::Rect bottom_gap(0, 10, 500, 100);
  AdjustBottomGapForRightSidePage(250, &bottom_gap);
  CompareRect({250, 10, 250, 100}, bottom_gap);

  bottom_gap.SetRect(15, 20, 700, 100);
  AdjustBottomGapForRightSidePage(365, &bottom_gap);
  CompareRect({365, 20, 350, 100}, bottom_gap);

  bottom_gap.SetRect(100, 40, 951, 200);
  AdjustBottomGapForRightSidePage(450, &bottom_gap);
  CompareRect({450, 40, 475, 200}, bottom_gap);
}

TEST(CoordinateTest, CenterRectHorizontally) {
  pp::Rect page_rect(10, 20, 400, 300);
  CenterRectHorizontally(600, &page_rect);
  CompareRect({100, 20, 400, 300}, page_rect);

  page_rect.SetRect(300, 450, 500, 700);
  CenterRectHorizontally(800, &page_rect);
  CompareRect({150, 450, 500, 700}, page_rect);

  page_rect.SetRect(800, 100, 200, 250);
  CenterRectHorizontally(350, &page_rect);
  CompareRect({75, 100, 200, 250}, page_rect);
}

TEST(CoordinateTest, ExpandDocumentSize) {
  pp::Size doc_size(100, 400);

  // Test various expansion sizes.
  pp::Size rect_size(100, 200);
  ExpandDocumentSize(rect_size, &doc_size);
  CompareSize({100, 600}, doc_size);

  rect_size.SetSize(200, 150);
  ExpandDocumentSize(rect_size, &doc_size);
  CompareSize({200, 750}, doc_size);

  rect_size.SetSize(100, 300);
  ExpandDocumentSize(rect_size, &doc_size);
  CompareSize({200, 1050}, doc_size);

  rect_size.SetSize(250, 400);
  ExpandDocumentSize(rect_size, &doc_size);
  CompareSize({250, 1450}, doc_size);
}

TEST(CoordinateTest, GetBottomGapBetweenRects) {
  CompareRect({95, 600, 350, 50},
              GetBottomGapBetweenRects(600, {95, 200, 350, 450}));

  CompareRect({200, 500, 350, 10},
              GetBottomGapBetweenRects(500, {200, 0, 350, 510}));

  // Test rectangle with a negative bottom value.
  CompareRect({150, -100, 400, 150},
              GetBottomGapBetweenRects(-100, {150, 0, 400, 50}));

  // Test case where |page_rect_bottom| >= |dirty_rect.bottom()|.
  CompareRect({0, 0, 0, 0}, GetBottomGapBetweenRects(1400, {0, 10, 300, 500}));
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

TEST(CoordinateTest, GetRectForSingleView) {
  // Test portrait pages.
  CompareRect({55, 503, 190, 390},
              GetRectForSingleView({200, 400}, {300, 500}, kSingleViewInsets));
  CompareRect({55, 603, 90, 330},
              GetRectForSingleView({100, 340}, {200, 600}, kSingleViewInsets));

  // Test landscape pages.
  CompareRect({5, 1003, 490, 440},
              GetRectForSingleView({500, 450}, {500, 1000}, kSingleViewInsets));
  CompareRect({30, 1503, 640, 190},
              GetRectForSingleView({650, 200}, {700, 1500}, kSingleViewInsets));
}

TEST(CoordinateTest, GetScreenRect) {
  const pp::Rect rect(10, 20, 200, 300);

  // Test various zooms with the position at the origin.
  CompareRect({10, 20, 200, 300}, GetScreenRect(rect, {0, 0}, 1));
  CompareRect({15, 30, 300, 450}, GetScreenRect(rect, {0, 0}, 1.5));
  CompareRect({5, 10, 100, 150}, GetScreenRect(rect, {0, 0}, 0.5));

  // Test various zooms with the position elsewhere.
  CompareRect({-390, -10, 200, 300}, GetScreenRect(rect, {400, 30}, 1));
  CompareRect({-385, 0, 300, 450}, GetScreenRect(rect, {400, 30}, 1.5));
  CompareRect({-395, -20, 100, 150}, GetScreenRect(rect, {400, 30}, 0.5));

  // Test various zooms with a negative position.
  CompareRect({-90, 70, 200, 300}, GetScreenRect(rect, {100, -50}, 1));
  CompareRect({-85, 80, 300, 450}, GetScreenRect(rect, {100, -50}, 1.5));
  CompareRect({-95, 60, 100, 150}, GetScreenRect(rect, {100, -50}, 0.5));

  // Test an empty rect always outputs an empty rect.
  const pp::Rect empty_rect;
  CompareRect({-20, -500, 0, 0}, GetScreenRect(empty_rect, {20, 500}, 1));
  CompareRect({-20, -500, 0, 0}, GetScreenRect(empty_rect, {20, 500}, 1.5));
  CompareRect({-20, -500, 0, 0}, GetScreenRect(empty_rect, {20, 500}, 0.5));
}

TEST(CoordinateTest, GetSurroundingRect) {
  constexpr int kDocWidth = 1000;

  // Test various position, sizes, and document width.
  CompareRect({0, 97, 1000, 314},
              GetSurroundingRect(100, 300, kSingleViewInsets, kDocWidth,
                                 kBottomSeparator));
  CompareRect({0, 37, 1000, 214},
              GetSurroundingRect(40, 200, kSingleViewInsets, kDocWidth,
                                 kBottomSeparator));
  CompareRect({0, 197, 1000, 514},
              GetSurroundingRect(200, 500, kSingleViewInsets, kDocWidth,
                                 kBottomSeparator));
  CompareRect(
      {0, -103, 200, 314},
      GetSurroundingRect(-100, 300, kSingleViewInsets, 200, kBottomSeparator));
}

TEST(CoordinateTest, GetLeftFillRect) {
  // Testing various rectangles with different positions and sizes.
  pp::Rect page_rect(10, 20, 400, 500);
  CompareRect({0, 17, 5, 514},
              GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  CompareRect({0, 297, 195, 364},
              GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(800, 650, 20, 15);
  CompareRect({0, 647, 795, 29},
              GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  CompareRect({0, -203, 45, 314},
              GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));
}

TEST(CoordinateTest, GetRightFillRect) {
  constexpr int kDocWidth = 1000;

  // Testing various rectangles with different positions, sizes, and document
  // widths.
  pp::Rect page_rect(10, 20, 400, 500);
  CompareRect({415, 17, 585, 514},
              GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  CompareRect({605, 297, 395, 364},
              GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  CompareRect(
      {605, 297, 195, 364},
      GetRightFillRect(page_rect, kSingleViewInsets, 800, kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  CompareRect({155, -203, 845, 314},
              GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));
}

TEST(CoordinateTest, GetBottomFillRect) {
  // Testing various rectangles with different positions and sizes.
  pp::Rect page_rect(10, 20, 400, 500);
  CompareRect({5, 527, 410, 4}, GetBottomFillRect(page_rect, kSingleViewInsets,
                                                  kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  CompareRect(
      {195, 657, 410, 4},
      GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(800, 650, 20, 15);
  CompareRect({795, 672, 30, 4}, GetBottomFillRect(page_rect, kSingleViewInsets,
                                                   kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  CompareRect({45, 107, 110, 4}, GetBottomFillRect(page_rect, kSingleViewInsets,
                                                   kBottomSeparator));
}

TEST(CoordinateTest, GetLeftRectForTwoUpView) {
  CompareRect({105, 103, 194, 390},
              GetLeftRectForTwoUpView({200, 400}, {300, 100}, kLeftInsets));
  CompareRect({5, 3, 294, 390},
              GetLeftRectForTwoUpView({300, 400}, {300, 0}, kLeftInsets));

  // Test rect smaller than shadow insets returns empty rect.
  CompareRect({10, 3, 0, 0},
              GetLeftRectForTwoUpView({5, 5}, {10, 0}, kLeftInsets));

  // Test empty rect gets positioned.
  CompareRect({105, 3, 0, 0},
              GetLeftRectForTwoUpView({0, 0}, {100, 0}, kLeftInsets));
}

TEST(CoordinateTest, GetRightRectForTwoUpView) {
  CompareRect({301, 103, 194, 390},
              GetRightRectForTwoUpView({200, 400}, {300, 100}, kRightInsets));
  CompareRect({301, 3, 294, 390},
              GetRightRectForTwoUpView({300, 400}, {300, 0}, kRightInsets));

  // Test rect smaller than shadow insets returns empty rect.
  CompareRect({11, 3, 0, 0},
              GetRightRectForTwoUpView({5, 5}, {10, 0}, kRightInsets));

  // Test empty rect gets positioned.
  CompareRect({101, 3, 0, 0},
              GetRightRectForTwoUpView({0, 0}, {100, 0}, kRightInsets));
}

}  // namespace draw_utils
}  // namespace chrome_pdf
