// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOOKUP_H_
#define UI_BASE_CURSOR_CURSOR_LOOKUP_H_

#include "ui/base/ui_base_export.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;

SkBitmap UI_BASE_EXPORT GetCursorBitmap(const Cursor& cursor);

gfx::Point UI_BASE_EXPORT GetCursorHotspot(const Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOOKUP_H_
