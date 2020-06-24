// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CURSOR_FACTORY_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CURSOR_FACTORY_OZONE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_theme_manager.h"
#include "ui/base/cursor/cursor_theme_manager_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/gfx/x/x11.h"

namespace ui {
class X11CursorOzone;

// CursorFactoryOzone implementation for X11 cursors.
class X11CursorFactoryOzone : public CursorFactory,
                              public CursorThemeManagerObserver {
 public:
  X11CursorFactoryOzone();
  ~X11CursorFactoryOzone() override;

  // CursorFactoryOzone:
  PlatformCursor GetDefaultCursor(mojom::CursorType type) override;
  PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  PlatformCursor CreateAnimatedCursor(const std::vector<SkBitmap>& bitmaps,
                                      const gfx::Point& hotspot,
                                      int frame_delay_ms) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;
  void ObserveThemeChanges() override;

 private:
  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  void ClearThemeCursors();

  // Loads/caches default cursor or returns cached version.
  scoped_refptr<X11CursorOzone> GetDefaultCursorInternal(
      mojom::CursorType type);

  // Holds a single instance of the invisible cursor. X11 has no way to hide
  // the cursor so an invisible cursor mimics that.
  scoped_refptr<X11CursorOzone> invisible_cursor_;

  std::map<mojom::CursorType, scoped_refptr<X11CursorOzone>> default_cursors_;

  ScopedObserver<CursorThemeManager, CursorThemeManagerObserver>
      cursor_theme_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(X11CursorFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CURSOR_FACTORY_OZONE_H_
