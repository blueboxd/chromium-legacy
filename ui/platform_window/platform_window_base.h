// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_BASE_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_BASE_H_

#include <memory>

#include "base/strings/string16.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

// Platform window.
//
// Each instance of PlatformWindowBase represents a single window in the
// underlying platform windowing system (i.e. X11/Win/OSX).
class PlatformWindowBase : public PropertyHandler {
 public:
  PlatformWindowBase();
  ~PlatformWindowBase() override;

  // PlatformWindow may be called with the |inactive| set to true in some cases.
  // That means that the Window Manager must not activate the window when it is
  // shown.  Most of PlatformWindow may ignore this value if not supported.
  virtual void Show(bool inactive = false) = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;

  virtual bool IsVisible() const = 0;

  // Informs the window it is going to be destroyed sometime soon. This is only
  // called for specific code paths, for example by Ash, so it shouldn't be
  // assumed this will get called before destruction.
  virtual void PrepareForShutdown() = 0;

  // Sets and gets the bounds of the platform-window. Note that the bounds is in
  // physical pixel coordinates.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetBounds() = 0;

  virtual void SetTitle(const base::string16& title) = 0;

  virtual void SetCapture() = 0;
  virtual void ReleaseCapture() = 0;
  virtual bool HasCapture() const = 0;

  virtual void ToggleFullscreen() = 0;
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;
  virtual PlatformWindowState GetPlatformWindowState() const = 0;

  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

  // Sets whether the window should have the standard title bar provided by the
  // underlying windowing system.  For the main browser window, this may be
  // changed by the user at any time via 'Show system title bar' option in the
  // tab strip menu.
  virtual void SetUseNativeFrame(bool use_native_frame) = 0;
  virtual bool ShouldUseNativeFrame() const = 0;

  virtual void SetCursor(PlatformCursor cursor) = 0;

  // Moves the cursor to |location|. Location is in platform window coordinates.
  virtual void MoveCursorTo(const gfx::Point& location) = 0;

  // Confines the cursor to |bounds| when it is in the platform window. |bounds|
  // is in platform window coordinates.
  virtual void ConfineCursorToBounds(const gfx::Rect& bounds) = 0;

  // Sets and gets the restored bounds of the platform-window.
  virtual void SetRestoredBoundsInPixels(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetRestoredBoundsInPixels() const = 0;

  // Tells if the content of the platform window should be transparent. By
  // default returns false.
  virtual bool ShouldWindowContentsBeTransparent() const;

  // Sets and gets ZOrderLevel of the PlatformWindow. Such platforms that do not
  // support ordering, should not implement these methods as the default
  // implementation always returns ZOrderLevel::kNormal value.
  virtual void SetZOrderLevel(ZOrderLevel order);
  virtual ZOrderLevel GetZOrderLevel() const;

  // Asks the PlatformWindow to stack itself on top of |widget|.
  virtual void StackAbove(gfx::AcceleratedWidget widget);
  virtual void StackAtTop();

  // Flashes the frame of the window to draw attention to it. If |flash_frame|
  // is set, the PlatformWindow must draw attention to it. If |flash_frame| is
  // not set, flashing must be stopped.
  virtual void FlashFrame(bool flash_frame);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_BASE_H_
