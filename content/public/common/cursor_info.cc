// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cursor_info.h"

#include "ui/base/cursor/cursor.h"
#include "ui/gfx/skia_util.h"

namespace content {

CursorInfo::CursorInfo(ui::mojom::CursorType cursor) : type(cursor) {}

CursorInfo::CursorInfo(const ui::Cursor& cursor)
    : type(cursor.type()), image_scale_factor(cursor.image_scale_factor()) {
  if (type == ui::mojom::CursorType::kCustom) {
    custom_image = cursor.custom_bitmap();
    hotspot = cursor.custom_hotspot();
  }
}

bool CursorInfo::operator==(const CursorInfo& other) const {
  return type == other.type && image_scale_factor == other.image_scale_factor &&
         (type != ui::mojom::CursorType::kCustom ||
          (hotspot == other.hotspot &&
           gfx::BitmapsAreEqual(custom_image, other.custom_image)));
}

ui::Cursor CursorInfo::GetCursor() const {
  ui::Cursor cursor(type);
  cursor.set_image_scale_factor(image_scale_factor);
  if (type == ui::mojom::CursorType::kCustom) {
    cursor.set_custom_hotspot(hotspot);
    cursor.set_custom_bitmap(custom_image);
  }
  return cursor;
}

}  // namespace content
