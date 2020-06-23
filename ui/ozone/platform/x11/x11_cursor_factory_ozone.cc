// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"

namespace ui {

namespace {

X11CursorOzone* ToX11CursorOzone(PlatformCursor cursor) {
  return static_cast<X11CursorOzone*>(cursor);
}

PlatformCursor ToPlatformCursor(X11CursorOzone* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

}  // namespace

X11CursorFactoryOzone::X11CursorFactoryOzone()
    : invisible_cursor_(X11CursorOzone::CreateInvisible()) {}

X11CursorFactoryOzone::~X11CursorFactoryOzone() {}

PlatformCursor X11CursorFactoryOzone::GetDefaultCursor(mojom::CursorType type) {
  return ToPlatformCursor(GetDefaultCursorInternal(type).get());
}

PlatformCursor X11CursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  // There is a problem with custom cursors that have no custom data. The
  // resulting SkBitmap is empty and X crashes when creating a zero size cursor
  // image. Return invisible cursor here instead.
  if (bitmap.drawsNothing()) {
    // The result of |invisible_cursor_| is owned by the caller, and will be
    // Unref()ed by code far away. (Usually in web_cursor.cc in content, among
    // others.) If we don't manually add another reference before we cast this
    // to a void*, we can end up with |invisible_cursor_| being freed out from
    // under us.
    invisible_cursor_->AddRef();
    return ToPlatformCursor(invisible_cursor_.get());
  }

  X11CursorOzone* cursor = new X11CursorOzone(bitmap, hotspot);
  cursor->AddRef();
  return ToPlatformCursor(cursor);
}

PlatformCursor X11CursorFactoryOzone::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  X11CursorOzone* cursor = new X11CursorOzone(bitmaps, hotspot, frame_delay_ms);
  cursor->AddRef();
  return ToPlatformCursor(cursor);
}

void X11CursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  ToX11CursorOzone(cursor)->AddRef();
}

void X11CursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  ToX11CursorOzone(cursor)->Release();
}

scoped_refptr<X11CursorOzone> X11CursorFactoryOzone::GetDefaultCursorInternal(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return invisible_cursor_;

  if (!default_cursors_.count(type)) {
    // Try to load a predefined X11 cursor.
    ::Cursor xcursor = LoadCursorFromType(type);
    if (xcursor == x11::None)
      return nullptr;
    auto cursor = base::MakeRefCounted<X11CursorOzone>(xcursor);
    default_cursors_[type] = cursor;
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type];
}

}  // namespace ui
