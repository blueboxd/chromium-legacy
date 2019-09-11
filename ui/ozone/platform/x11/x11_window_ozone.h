// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_

#include <array>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/x11/x11_window.h"

namespace ui {

class X11WindowManagerOzone;
struct PlatformWindowInitProperties;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11WindowOzone : public X11Window,
                       public WmMoveResizeHandler,
                       public XEventDispatcher {
 public:
  X11WindowOzone(PlatformWindowDelegate* delegate,
                 X11WindowManagerOzone* window_manager);
  ~X11WindowOzone() override;

  gfx::AcceleratedWidget widget() const { return widget_; }

  // Called by |window_manager_| once capture is set to another X11WindowOzone.
  void OnLostCapture();

  // Overridden from PlatformWindow:
  void Close() override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetCursor(PlatformCursor cursor) override;

  void OnMouseEnter();

  // Overridden from ui::XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override;
  void PlatformEventDispatchFinished() override;
  PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(XEvent* event) override;

 private:
  // XWindow overrides:
  void OnXWindowCreated() override;

  // X11Window overrides:
  void SetPlatformEventDispatcher() override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location) override;

  void Init(const PlatformWindowInitProperties& params);
  void SetWidget(XID xwindow);
  void RemoveFromWindowManager();

  X11WindowManagerOzone* const window_manager_;

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  bool is_shutting_down_ = false;

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  bool handle_next_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
