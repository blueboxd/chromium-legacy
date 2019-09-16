// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_

#include "base/macros.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

// Delegate interface used to communicate the X11PlatformWindow API client about
// XEvents of interest.
class X11_WINDOW_EXPORT XEventDelegate {
 public:
  virtual ~XEventDelegate() {}

  // TODO(crbug.com/990756): We need to implement/reuse ozone interface for
  // these.
  virtual void OnXWindowSelectionEvent(XEvent* xev) = 0;
  virtual void OnXWindowDragDropEvent(XEvent* xev) = 0;

  // TODO(crbug.com/981606): DesktopWindowTreeHostX11 forward raw |XEvent|s to
  // ATK components that currently live in views layer.  Remove once ATK code
  // is reworked to be reusable.
  virtual void OnXWindowRawKeyEvent(XEvent* xev) = 0;
};

// PlatformWindow implementation for X11. PlatformEvents are XEvents.
class X11_WINDOW_EXPORT X11Window : public PlatformWindow,
                                    public WmMoveResizeHandler,
                                    public XWindow,
                                    public PlatformEventDispatcher {
 public:
  explicit X11Window(PlatformWindowDelegateLinux* platform_window_delegate);
  ~X11Window() override;

  void Initialize(PlatformWindowInitProperties properties);

  void SetXEventDelegate(XEventDelegate* delegate);

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;

 protected:
  PlatformWindowDelegateLinux* platform_window_delegate() const {
    return platform_window_delegate_;
  }

  // XWindow:
  void OnXWindowCreated() override;
  void OnXWindowLostCapture() override;

 private:
  void ProcessXInput2Event(XEvent* xev);

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // XWindow:
  void OnXWindowStateChanged() override;
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect) override;
  void OnXWindowBoundsChanged(const gfx::Rect& size) override;
  void OnXWindowCloseRequested() override;
  void OnXWindowIsActiveChanged(bool active) override;
  void OnXWindowMapped() override;
  void OnXWindowUnmapped() override;
  void OnXWindowWorkspaceChanged() override;
  void OnXWindowLostPointerGrab() override;
  void OnXWindowEvent(ui::Event* event) override;
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;
  void OnXWindowRawKeyEvent(XEvent* xev) override;
  base::Optional<gfx::Size> GetMinimumSizeForXWindow() override;
  base::Optional<gfx::Size> GetMaximumSizeForXWindow() override;

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location) override;

  // X11WindowOzone sets own event dispatcher now.
  virtual void SetPlatformEventDispatcher();

  // Adjusts |requested_size_in_pixels| to avoid the WM "feature" where setting
  // the window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSizeForDisplay(const gfx::Size& requested_size_in_pixels);

  // Stores current state of this window.
  PlatformWindowState state_ = PlatformWindowState::kUnknown;

  PlatformWindowDelegateLinux* const platform_window_delegate_;

  XEventDelegate* x_event_delegate_ = nullptr;

  // Tells if the window got a ::Close call.
  bool is_shutting_down_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  // The bounds of our window before the window was maximized.
  gfx::Rect restored_bounds_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(X11Window);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
