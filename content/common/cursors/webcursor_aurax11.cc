// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/check_op.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11.h"

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor(const ui::Cursor& cursor) {
  // The other cursor types are set in CursorLoaderX11
  DCHECK_EQ(cursor.type(), ui::mojom::CursorType::kCustom);

  if (platform_cursor_)
    return platform_cursor_;

  XcursorImage* image = ui::SkBitmapToXcursorImage(cursor.custom_bitmap(),
                                                   cursor.custom_hotspot());
  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::UnrefCustomXCursor(platform_cursor_);
    platform_cursor_ = 0;
  }
  custom_cursor_.reset();
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::RefCustomXCursor(platform_cursor_);

  device_scale_factor_ = other.device_scale_factor_;
}

}  // namespace content
