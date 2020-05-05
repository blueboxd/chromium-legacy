// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_cursor_ozone.h"

#include "base/check_op.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

X11CursorOzone::X11CursorOzone(const SkBitmap& bitmap,
                               const gfx::Point& hotspot) {
  XcursorImage* image = SkBitmapToXcursorImage(bitmap, hotspot);
  xcursor_ = XcursorImageLoadCursor(gfx::GetXDisplay(), image);
  XcursorImageDestroy(image);
}

X11CursorOzone::X11CursorOzone(const std::vector<SkBitmap>& bitmaps,
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

X11CursorOzone::X11CursorOzone(const char* name) {
  xcursor_ = XcursorLibraryLoadCursor(gfx::GetXDisplay(), name);
}

// static
scoped_refptr<X11CursorOzone> X11CursorOzone::CreateInvisible() {
  scoped_refptr<X11CursorOzone> invisible_ = new X11CursorOzone();
  invisible_->xcursor_ = CreateInvisibleCursor();
  return invisible_;
}

X11CursorOzone::X11CursorOzone() {}

X11CursorOzone::~X11CursorOzone() {
  XFreeCursor(gfx::GetXDisplay(), xcursor_);
}

}  // namespace ui
