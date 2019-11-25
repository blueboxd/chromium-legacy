// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class PlatformWindow;
class X11ExtensionDelegate;

// Linux extensions that linux platforms can use to extend the platform windows
// APIs. Please refer to README for more details.
class COMPONENT_EXPORT(EXTENSIONS) X11Extension {
 public:
  // X11-specific.  Returns whether an XSync extension is available at the
  // current platform.
  virtual bool IsSyncExtensionAvailable() const = 0;

  // X11-specific.  Handles CompleteSwapAfterResize event coming from the
  // compositor observer.
  virtual void OnCompleteSwapAfterResize() = 0;

  // X11-specific.  Returns the current bounds in terms of the X11 Root Window
  // including the borders provided by the window manager (if any).
  virtual gfx::Rect GetXRootWindowOuterBounds() const = 0;

  // X11-specific.  Says if the X11 Root Window contains the point within its
  // set shape. If shape is not set, returns true.
  virtual bool ContainsPointInXRegion(const gfx::Point& point) const = 0;

  // X11-specific.  Asks X11 to lower the Xwindow down the stack so that it does
  // not obscure any sibling windows.
  virtual void LowerXWindow() = 0;

  virtual void SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) = 0;

 protected:
  virtual ~X11Extension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetX11Extension(PlatformWindow* platform_window,
                       X11Extension* x11_extensions);
};

COMPONENT_EXPORT(EXTENSIONS)
X11Extension* GetX11Extension(const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_
