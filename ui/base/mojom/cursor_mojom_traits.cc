// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojom/cursor_mojom_traits.h"

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/mojom/cursor.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
gfx::Point StructTraits<ui::mojom::CursorDataView, ui::Cursor>::hotspot(
    const ui::Cursor& c) {
  return c.custom_hotspot();
}

// static
SkBitmap StructTraits<ui::mojom::CursorDataView, ui::Cursor>::bitmap(
    const ui::Cursor& c) {
  return c.custom_bitmap();
}

// static
bool StructTraits<ui::mojom::CursorDataView, ui::Cursor>::Read(
    ui::mojom::CursorDataView data,
    ui::Cursor* out) {
  ui::mojom::CursorType type;
  if (!data.ReadType(&type))
    return false;

  if (type != ui::mojom::CursorType::kCustom) {
    *out = ui::Cursor(type);
    return true;
  }

  gfx::Point hotspot;
  SkBitmap bitmap;

  if (!data.ReadHotspot(&hotspot) || !data.ReadBitmap(&bitmap))
    return false;

  *out = ui::Cursor(type);
  out->set_custom_bitmap(bitmap);
  out->set_custom_hotspot(hotspot);
  out->set_image_scale_factor(data.image_scale_factor());
  return true;
}

}  // namespace mojo
