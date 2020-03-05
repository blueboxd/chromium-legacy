// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "base/logging.h"
#include "ui/gfx/skia_util.h"

namespace ui {

Cursor::Cursor() = default;

Cursor::Cursor(mojom::CursorType type) : type_(type) {}

Cursor::Cursor(const Cursor& cursor)
    : type_(cursor.type_),
      platform_cursor_(cursor.platform_cursor_),
      image_scale_factor_(cursor.image_scale_factor_) {
  if (type_ == mojom::CursorType::kCustom) {
    custom_hotspot_ = cursor.custom_hotspot_;
    custom_bitmap_ = cursor.custom_bitmap_;
    RefCustomCursor();
  }
}

Cursor::~Cursor() {
  if (type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
}

void Cursor::SetPlatformCursor(const PlatformCursor& platform) {
  if (type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
  platform_cursor_ = platform;
  if (type_ == mojom::CursorType::kCustom)
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
  return type_ == cursor.type_ && platform_cursor_ == cursor.platform_cursor_ &&
         image_scale_factor_ == cursor.image_scale_factor_ &&
         (type_ != mojom::CursorType::kCustom ||
          (custom_hotspot_ == cursor.custom_hotspot_ &&
           gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_)));
}

void Cursor::operator=(const Cursor& cursor) {
  if (*this == cursor)
    return;
  if (type_ == mojom::CursorType::kCustom)
    UnrefCustomCursor();
  type_ = cursor.type_;
  platform_cursor_ = cursor.platform_cursor_;
  if (type_ == mojom::CursorType::kCustom) {
    RefCustomCursor();
    custom_hotspot_ = cursor.custom_hotspot_;
    custom_bitmap_ = cursor.custom_bitmap_;
  }
  image_scale_factor_ = cursor.image_scale_factor_;
}

}  // namespace ui
