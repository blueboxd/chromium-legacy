// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "base/logging.h"
#include "ui/gfx/skia_util.h"

namespace ui {

Cursor::Cursor() = default;

Cursor::Cursor(mojom::CursorType type) : native_type_(type) {}

Cursor::Cursor(const Cursor& cursor)
    : native_type_(cursor.native_type_),
      platform_cursor_(cursor.platform_cursor_),
      device_scale_factor_(cursor.device_scale_factor_) {
  if (native_type_ == mojom::CursorType::kCustom) {
    custom_hotspot_ = cursor.custom_hotspot_;
    custom_bitmap_ = cursor.custom_bitmap_;
    RefCustomCursor();
  }
}

Cursor::~Cursor() {
  if (native_type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
}

void Cursor::SetPlatformCursor(const PlatformCursor& platform) {
  if (native_type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
  platform_cursor_ = platform;
  if (native_type_ == mojom::CursorType::kCustom)
    RefCustomCursor();
}

#if !defined(USE_AURA)
void Cursor::RefCustomCursor() {
  NOTIMPLEMENTED();
}
void Cursor::UnrefCustomCursor() {
  NOTIMPLEMENTED();
}
#endif

bool Cursor::operator==(const Cursor& cursor) const {
  return native_type_ == cursor.native_type_ &&
         platform_cursor_ == cursor.platform_cursor_ &&
         device_scale_factor_ == cursor.device_scale_factor_ &&
         (native_type_ != mojom::CursorType::kCustom ||
          (custom_hotspot_ == cursor.custom_hotspot_ &&
           gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_)));
}

void Cursor::operator=(const Cursor& cursor) {
  if (*this == cursor)
    return;
  if (native_type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
  native_type_ = cursor.native_type_;
  platform_cursor_ = cursor.platform_cursor_;
  if (native_type_ == mojom::CursorType::kCustom) {
    RefCustomCursor();
    custom_hotspot_ = cursor.custom_hotspot_;
    custom_bitmap_ = cursor.custom_bitmap_;
  }
  device_scale_factor_ = cursor.device_scale_factor_;
}

}  // namespace ui
