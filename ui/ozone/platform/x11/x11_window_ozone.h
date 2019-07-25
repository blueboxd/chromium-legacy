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
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class X11WindowManagerOzone;
struct PlatformWindowInitProperties;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11WindowOzone : public PlatformWindow,
                       public PlatformEventDispatcher,
                       public XEventDispatcher,
                       public XWindow::Delegate {
 public:
  X11WindowOzone(PlatformWindowDelegate* delegate,
                 const PlatformWindowInitProperties& properties,
                 X11WindowManagerOzone* window_manager);
  ~X11WindowOzone() override;

  gfx::AcceleratedWidget widget() const { return widget_; }
  bool IsVisible() const;
  gfx::Rect GetOutterBounds() const;
  ::Region GetShape() const;

  // Called by |window_manager_| once capture is set to another X11WindowOzone.
  void OnLostCapture();

  // Overridden from PlatformWindow:
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

  // Overridden from ui::XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override;
  void PlatformEventDispatchFinished() override;
  PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(XEvent* event) override;

 private:
  // Overridden from ui::XWindow::Delegate
  void OnXWindowCreated() override;
  void OnXWindowStateChanged() override;
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect) override;
  void OnXWindowSizeChanged(const gfx::Size& size) override;
  void OnXWindowCloseRequested() override;
  void OnXWindowLostCapture() override;
  void OnXWindowIsActiveChanged(bool active) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void Init(const PlatformWindowInitProperties& params);
  void SetWidget(XID xwindow);
  void RemoveFromWindowManager();

  PlatformWindowDelegate* const delegate_;
  X11WindowManagerOzone* const window_manager_;

  PlatformWindowState state_ = PlatformWindowState::kUnknown;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  std::unique_ptr<ui::XWindow> x11_window_;

  bool is_shutting_down_ = false;

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  bool handle_next_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
