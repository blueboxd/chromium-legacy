// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

X11Cursor* ToX11Cursor(PlatformCursor cursor) {
  return static_cast<X11Cursor*>(cursor);
}

PlatformCursor ToPlatformCursor(X11Cursor* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

}  // namespace

X11CursorFactory::X11CursorFactory()
    : invisible_cursor_(X11Cursor::CreateInvisible()) {}

X11CursorFactory::~X11CursorFactory() = default;

base::Optional<PlatformCursor> X11CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  auto cursor = GetDefaultCursorInternal(type);
  if (!cursor)
    return base::nullopt;
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateImageCursor(const SkBitmap& bitmap,
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

  auto cursor = base::MakeRefCounted<X11Cursor>(bitmap, hotspot);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  auto cursor =
      base::MakeRefCounted<X11Cursor>(bitmaps, hotspot, frame_delay_ms);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

void X11CursorFactory::RefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->AddRef();
}

void X11CursorFactory::UnrefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->Release();
}

void X11CursorFactory::ObserveThemeChanges() {
  auto* cursor_theme_manager = CursorThemeManager::GetInstance();
  if (cursor_theme_manager)
    cursor_theme_observer_.Add(cursor_theme_manager);
}

void X11CursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  XcursorSetTheme(gfx::GetXDisplay(), cursor_theme_name.c_str());
  ClearThemeCursors();
}

void X11CursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  XcursorSetDefaultSize(gfx::GetXDisplay(), cursor_theme_size);
  ClearThemeCursors();
}

scoped_refptr<X11Cursor> X11CursorFactory::GetDefaultCursorInternal(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return invisible_cursor_;

  if (!default_cursors_.count(type)) {
    // Try to load a predefined X11 cursor.
    ::Cursor xcursor = LoadCursorFromType(type);
    if (xcursor == x11::None)
      return nullptr;
    auto cursor = base::MakeRefCounted<X11Cursor>(xcursor);
    default_cursors_[type] = cursor;
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type];
}

void X11CursorFactory::ClearThemeCursors() {
  default_cursors_.clear();
}

}  // namespace ui
