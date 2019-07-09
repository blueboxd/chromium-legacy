// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_

#include <array>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class X11WindowManagerOzone;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11WindowOzone : public PlatformWindow,
                       public PlatformEventDispatcher,
                       public XEventDispatcher {
 public:
  X11WindowOzone(X11WindowManagerOzone* window_manager,
                 PlatformWindowDelegate* delegate,
                 const gfx::Rect& bounds);
  ~X11WindowOzone() override;

  gfx::AcceleratedWidget widget() const { return widget_; }

  // Called by |window_manager_| once capture is set to another X11WindowOzone.
  void OnLostCapture();

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void Activate() override;
  void Deactivate() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetCursor(PlatformCursor cursor) override;

  // XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override;
  void PlatformEventDispatchFinished() override;
  PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(XEvent* event) override;

 private:
  void RemoveFromWindowManager();

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void Create();
  void Destroy();
  XID CreateXWindow();
  void SetXWindow(XID xwindow);
  bool IsEventForXWindow(const XEvent& xev) const;
  void ProcessXWindowEvent(XEvent* xev);
  void OnWMStateUpdated();
  void UnconfineCursor();

  X11WindowManagerOzone* window_manager_;
  PlatformWindowDelegate* const delegate_;
  XDisplay* xdisplay_;
  XID xroot_window_;
  XID xwindow_;
  bool mapped_;
  gfx::Rect bounds_;
  base::string16 window_title_;
  PlatformWindowState state_;
  base::flat_set<XAtom> window_properties_;

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  bool handle_next_event_ = false;
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  // Keep track of barriers to confine cursor.
  bool has_pointer_barriers_ = false;
  std::array<XID, 4> pointer_barriers_;

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
