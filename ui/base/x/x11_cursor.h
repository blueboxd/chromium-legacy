// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_CURSOR_H_
#define UI_BASE_X_X11_CURSOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/x11.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

// Ref counted class to hold an X11 cursor resource.  Clears the X11 resources
// on destruction
class COMPONENT_EXPORT(UI_BASE_X) X11Cursor
    : public base::RefCounted<X11Cursor> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // Handles creating X11 cursor resources from SkBitmap/hotspot.
  X11Cursor(const SkBitmap& bitmap, const gfx::Point& hotspot);
  X11Cursor(const X11Cursor&) = delete;
  X11Cursor& operator=(const X11Cursor&) = delete;
  X11Cursor(const std::vector<SkBitmap>& bitmaps,
            const gfx::Point& hotspot,
            int frame_delay_ms);
  // Wraps an X11 cursor |xcursor|.
  explicit X11Cursor(::Cursor xcursor);

  // Creates a new cursor that is invisible.
  static scoped_refptr<X11Cursor> CreateInvisible();

  ::Cursor xcursor() const { return xcursor_; }

 private:
  friend class base::RefCounted<X11Cursor>;

  ~X11Cursor();

  ::Cursor xcursor_ = x11::None;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_CURSOR_H_
