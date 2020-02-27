// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSORS_AURA_H_
#define UI_BASE_CURSOR_CURSORS_AURA_H_

#include "ui/base/cursor/cursor.h"
#include "ui/base/mojom/cursor_type.mojom-forward.h"
#include "ui/base/ui_base_export.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
enum class CursorSize;

const int kAnimatedCursorFrameDelayMs = 25;

// Returns CSS cursor name from an Aura cursor ID.
UI_BASE_EXPORT const char* CursorCssNameFromId(mojom::CursorType id);

// Returns data about |id|, where id is a cursor constant like
// ui::mojom::CursorType::kHelp. The IDR will be placed in |resource_id| and
// the hotspots for the different DPIs will be placed in |hot_1x| and
// |hot_2x|.  Returns false if |id| is invalid.
UI_BASE_EXPORT bool GetCursorDataFor(CursorSize cursor_size,
                                     mojom::CursorType id,
                                     float scale_factor,
                                     int* resource_id,
                                     gfx::Point* point);

// Like above, but for animated cursors.
UI_BASE_EXPORT bool GetAnimatedCursorDataFor(CursorSize cursor_size,
                                             mojom::CursorType id,
                                             float scale_factor,
                                             int* resource_id,
                                             gfx::Point* point);

SkBitmap GetDefaultBitmap(const Cursor& cursor);

gfx::Point GetDefaultHotspot(const Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSORS_AURA_H_
