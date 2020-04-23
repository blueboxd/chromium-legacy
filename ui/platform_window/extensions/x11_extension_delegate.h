// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_

#include "base/component_export.h"
#include "ui/base/buildflags.h"

#if BUILDFLAG(USE_ATK)
using AtkKeyEventStruct = struct _AtkKeyEventStruct;
#endif

class SkPath;

namespace gfx {
class Size;
}

namespace ui {

class COMPONENT_EXPORT(EXTENSIONS) X11ExtensionDelegate {
 public:
  // Notifies if the PlatformWindow looses a mouse grab. This can be useful
  // for Wayland or X11. Both of them provide pointer enter and leave
  // notifications, which non-ozone X11 (just an example) use to be using to
  // notify about lost pointer grab along with explicit grabs. Wayland also
  // has this technique. However, explicit grab is available only for popup
  // (menu) windows.
  virtual void OnLostMouseGrab() = 0;

  // Returns a mask to be used to clip the window for the given
  // size. This is used to create the non-rectangular window shape.
  virtual void GetWindowMask(const gfx::Size& size, SkPath* window_mask) = 0;

#if BUILDFLAG(USE_ATK)
  virtual bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event) = 0;
#endif

  // Returns true if this window should be in a forced override-redirect state
  // (not managed by the window manager). If |is_tiling_wm| is set to true, the
  // underlaying window manager is tiling. If it is set to false, the wm is
  // stacking. The delegate can use this information to determine the value
  // returned for override-redirect.
  virtual bool IsOverrideRedirect(bool is_tiling_wm) const = 0;

 protected:
  virtual ~X11ExtensionDelegate() = default;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_
