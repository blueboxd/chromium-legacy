// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor.h"

#include "base/check_op.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

X11Cursor::X11Cursor(const SkBitmap& bitmap, const gfx::Point& hotspot) {
  XcursorImage* image = SkBitmapToXcursorImage(bitmap, hotspot);
  xcursor_ = XcursorImageLoadCursor(gfx::GetXDisplay(), image);
  XcursorImageDestroy(image);
}

X11Cursor::X11Cursor(const std::vector<SkBitmap>& bitmaps,
                     const gfx::Point& hotspot,
                     int frame_delay_ms) {
  // Initialize an XCursorImage for each frame, store all of them in
  // XCursorImages and load the cursor from that.
  XcursorImages* images = XcursorImagesCreate(bitmaps.size());
  images->nimage = bitmaps.size();
  for (size_t frame = 0; frame < bitmaps.size(); ++frame) {
    XcursorImage* x_image = SkBitmapToXcursorImage(bitmaps[frame], hotspot);
    x_image->delay = frame_delay_ms;
    images->images[frame] = x_image;
  }

  xcursor_ = XcursorImagesLoadCursor(gfx::GetXDisplay(), images);
  XcursorImagesDestroy(images);
}

X11Cursor::X11Cursor(::Cursor xcursor) : xcursor_(xcursor) {}

// static
scoped_refptr<X11Cursor> X11Cursor::CreateInvisible() {
  return base::MakeRefCounted<X11Cursor>(CreateInvisibleCursor());
}

X11Cursor::~X11Cursor() {
  XFreeCursor(gfx::GetXDisplay(), xcursor_);
}

}  // namespace ui
