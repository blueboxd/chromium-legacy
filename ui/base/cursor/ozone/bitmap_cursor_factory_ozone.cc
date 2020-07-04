// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

#include <algorithm>

#include "base/check_op.h"

namespace ui {

namespace {

BitmapCursorOzone* ToBitmapCursorOzone(PlatformCursor cursor) {
  return static_cast<BitmapCursorOzone*>(cursor);
}

PlatformCursor ToPlatformCursor(BitmapCursorOzone* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

}  // namespace

BitmapCursorOzone::BitmapCursorOzone(const SkBitmap& bitmap,
                                     const gfx::Point& hotspot)
    : hotspot_(hotspot), frame_delay_ms_(0) {
  if (!bitmap.isNull())
    bitmaps_.push_back(bitmap);
}

BitmapCursorOzone::BitmapCursorOzone(const std::vector<SkBitmap>& bitmaps,
                                     const gfx::Point& hotspot,
                                     int frame_delay_ms)
    : bitmaps_(bitmaps), hotspot_(hotspot), frame_delay_ms_(frame_delay_ms) {
  DCHECK_LT(0U, bitmaps.size());
  DCHECK_LE(0, frame_delay_ms);
  // No null bitmap should be in the list. Blank cursors should just be an empty
  // vector.
  DCHECK(std::find_if(bitmaps_.begin(), bitmaps_.end(),
                      [](const SkBitmap& bitmap) { return bitmap.isNull(); }) ==
         bitmaps_.end());
}

BitmapCursorOzone::~BitmapCursorOzone() {
}

const gfx::Point& BitmapCursorOzone::hotspot() {
  return hotspot_;
}

const SkBitmap& BitmapCursorOzone::bitmap() {
  return bitmaps_[0];
}

const std::vector<SkBitmap>& BitmapCursorOzone::bitmaps() {
  return bitmaps_;
}

int BitmapCursorOzone::frame_delay_ms() {
  return frame_delay_ms_;
}

BitmapCursorFactoryOzone::BitmapCursorFactoryOzone() {}

BitmapCursorFactoryOzone::~BitmapCursorFactoryOzone() {}

// static
scoped_refptr<BitmapCursorOzone> BitmapCursorFactoryOzone::GetBitmapCursor(
    PlatformCursor platform_cursor) {
  return base::WrapRefCounted(ToBitmapCursorOzone(platform_cursor));
}

base::Optional<PlatformCursor> BitmapCursorFactoryOzone::GetDefaultCursor(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return nullptr;  // nullptr is used for the hidden cursor.
  return base::nullopt;
}

PlatformCursor BitmapCursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  BitmapCursorOzone* cursor = new BitmapCursorOzone(bitmap, hotspot);
  cursor->AddRef();  // Balanced by UnrefImageCursor.
  return ToPlatformCursor(cursor);
}

PlatformCursor BitmapCursorFactoryOzone::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  DCHECK_LT(0U, bitmaps.size());
  BitmapCursorOzone* cursor =
      new BitmapCursorOzone(bitmaps, hotspot, frame_delay_ms);
  cursor->AddRef();  // Balanced by UnrefImageCursor.
  return ToPlatformCursor(cursor);
}

void BitmapCursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->AddRef();
}

void BitmapCursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->Release();
}

}  // namespace ui
