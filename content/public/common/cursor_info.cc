// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cursor_info.h"

#include "ui/gfx/skia_util.h"

namespace content {

CursorInfo::CursorInfo(ui::mojom::CursorType cursor) : type(cursor) {}

CursorInfo::CursorInfo(const blink::WebCursorInfo& info)
    : type(info.type), image_scale_factor(info.image_scale_factor) {
  if (type == ui::mojom::CursorType::kCustom) {
    custom_image = info.custom_image;
    hotspot = info.hot_spot;
  }
}

bool CursorInfo::operator==(const CursorInfo& other) const {
  return type == other.type && image_scale_factor == other.image_scale_factor &&
         (type != ui::mojom::CursorType::kCustom ||
          (hotspot == other.hotspot &&
           gfx::BitmapsAreEqual(custom_image, other.custom_image)));
}

blink::WebCursorInfo CursorInfo::GetWebCursorInfo() const {
  blink::WebCursorInfo info;
  info.type = type;
  info.image_scale_factor = image_scale_factor;
  if (type == ui::mojom::CursorType::kCustom) {
    info.hot_spot = hotspot;
    info.custom_image = custom_image;
  }
  return info;
}

}  // namespace content
