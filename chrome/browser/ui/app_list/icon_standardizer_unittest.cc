// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/icon_standardizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using CreateStandardIconTest = testing::Test;

// Test that a square icon gets scaled down and drawn on top of a circular
// background when converted to a standard icon.
TEST_F(CreateStandardIconTest, SquareIconToStandardIcon) {
  const int test_width = 64;
  const int test_height = 64;

  SkBitmap square_icon_bitmap;
  square_icon_bitmap.allocN32Pixels(test_width, test_height);
  square_icon_bitmap.eraseColor(SK_ColorRED);

  gfx::ImageSkia standard_icon = app_list::CreateStandardIconImage(
      gfx::ImageSkia::CreateFrom1xBitmap(square_icon_bitmap));

  // Create |test_standard_bitmap| which will be a manually created standard
  // icon, with background circle and a scaled down square icon inside.
  SkBitmap test_standard_bitmap;
  test_standard_bitmap.allocN32Pixels(test_width, test_height);
  test_standard_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(test_standard_bitmap);

  SkPaint paint_background_circle;
  paint_background_circle.setAntiAlias(true);
  paint_background_circle.setColor(SK_ColorWHITE);
  paint_background_circle.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(
      SkPoint::Make((test_width - 1) / 2.0f, (test_width - 1) / 2.0f),
      test_width / 2.0f - test_width * 0.01f, paint_background_circle);

  const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
      square_icon_bitmap, skia::ImageOperations::RESIZE_BEST, 41, 41);
  canvas.drawBitmap(scaled_bitmap, 11, 11);

  // Test that |standard_icon| has an identical bitmap to
  // |test_standard_bitmap|.
  const size_t size = standard_icon.bitmap()->computeByteSize();
  bool bitmaps_equal = true;

  uint8_t* first_data =
      reinterpret_cast<uint8_t*>(standard_icon.bitmap()->getPixels());
  uint8_t* second_data =
      reinterpret_cast<uint8_t*>(test_standard_bitmap.getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i]) {
      bitmaps_equal = false;
      break;
    }
  }
  EXPECT_TRUE(bitmaps_equal);
}

// Test that a circular icon stays the same when converted to a standard icon.
TEST_F(CreateStandardIconTest, CircularIconToStandardIcon) {
  const int test_width = 64;
  const int test_height = 64;

  // Create a bitmap with a red circle as a placeholder circular icon.
  SkBitmap circle_icon_bitmap;
  circle_icon_bitmap.allocN32Pixels(test_width, test_height);
  circle_icon_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(circle_icon_bitmap);
  SkPaint paint_cirlce_icon;
  paint_cirlce_icon.setAntiAlias(true);
  paint_cirlce_icon.setColor(SK_ColorRED);
  paint_cirlce_icon.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(SkPoint::Make(test_width / 2, test_width / 2),
                    test_width / 2, paint_cirlce_icon);

  // Get the standard icon version of the red circle icon.
  gfx::ImageSkia standard_icon = app_list::CreateStandardIconImage(
      gfx::ImageSkia(gfx::ImageSkiaRep(circle_icon_bitmap, 2.0f)));

  // Test that |standard_icon| has an identical bitmap to  |circle_icon_bitmap|.
  const size_t size = standard_icon.bitmap()->computeByteSize();
  bool bitmaps_equal = true;

  uint8_t* first_data =
      reinterpret_cast<uint8_t*>(standard_icon.bitmap()->getPixels());
  uint8_t* second_data =
      reinterpret_cast<uint8_t*>(circle_icon_bitmap.getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i]) {
      bitmaps_equal = false;
      break;
    }
  }
  EXPECT_TRUE(bitmaps_equal);
}
