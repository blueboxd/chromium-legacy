// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor_lookup.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/skia_util.h"

namespace ui {
namespace {

TEST(CursorTest, Null) {
  Cursor cursor;
  EXPECT_EQ(mojom::CursorType::kNull, cursor.type());
}

TEST(CursorTest, BasicType) {
  Cursor cursor(mojom::CursorType::kPointer);
  EXPECT_EQ(mojom::CursorType::kPointer, cursor.type());

  Cursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomType) {
  Cursor cursor(mojom::CursorType::kCustom);
  EXPECT_EQ(mojom::CursorType::kCustom, cursor.type());

  const float kScale = 2.0f;
  cursor.set_image_scale_factor(kScale);
  EXPECT_EQ(kScale, cursor.image_scale_factor());

  const gfx::Point kHotspot = gfx::Point(5, 2);
  cursor.set_custom_hotspot(kHotspot);
  EXPECT_EQ(kHotspot, GetCursorHotspot(cursor));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);
  cursor.set_custom_bitmap(bitmap);

  EXPECT_EQ(bitmap.getGenerationID(),
            GetCursorBitmap(cursor).getGenerationID());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, GetCursorBitmap(cursor)));

  Cursor copy(cursor);
  EXPECT_EQ(GetCursorBitmap(cursor).getGenerationID(),
            GetCursorBitmap(copy).getGenerationID());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(GetCursorBitmap(cursor), GetCursorBitmap(copy)));
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomTypeComparesBitmapPixels) {
  Cursor cursor1(mojom::CursorType::kCustom);
  Cursor cursor2(mojom::CursorType::kCustom);

  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(10, 10);
  bitmap1.eraseColor(SK_ColorRED);
  cursor1.set_custom_bitmap(bitmap1);

  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(10, 10);
  bitmap2.eraseColor(SK_ColorRED);
  cursor2.set_custom_bitmap(bitmap2);

  EXPECT_NE(GetCursorBitmap(cursor1).getGenerationID(),
            GetCursorBitmap(cursor2).getGenerationID());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(GetCursorBitmap(cursor1), GetCursorBitmap(cursor2)));
  EXPECT_EQ(cursor1, cursor2);
}

}  // namespace
}  // namespace ui
