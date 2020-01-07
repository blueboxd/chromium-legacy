// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"

namespace ui {

class ShellSurfaceWrapper;

class WaylandSurface : public WaylandWindow,
                       public WmMoveResizeHandler,
                       public WmDragHandler {
 public:
  WaylandSurface(PlatformWindowDelegate* delegate,
                 WaylandConnection* connection);
  ~WaylandSurface() override;

  ShellSurfaceWrapper* shell_surface() const { return shell_surface_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // WmDragHandler
  void StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override;

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetTitle(const base::string16& title) override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void SizeConstraintsChanged() override;

 private:
  // WaylandWindow overrides:
  void HandleSurfaceConfigure(int32_t widht,
                              int32_t height,
                              bool is_maximized,
                              bool is_fullscreen,
                              bool is_activated) override;
  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation) override;
  int OnDragMotion(const gfx::PointF& point,
                   uint32_t time,
                   int operation) override;
  void OnDragDrop(std::unique_ptr<OSExchangeData> data) override;
  void OnDragLeave() override;
  void OnDragSessionClose(uint32_t dnd_action) override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;

  void TriggerStateChanges();
  void SetWindowState(PlatformWindowState state);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellSurface();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the |min_size_| and |max_size_| to the ShellSurface.
  void SetSizeConstraints();

  void SetOrResetRestoredBounds();

  // Wrappers around shell surface.
  std::unique_ptr<ShellSurfaceWrapper> shell_surface_;

  base::OnceCallback<void(int)> drag_closed_callback_;

  // These bounds attributes below have suffices that indicate units used.
  // Wayland operates in DIP but the platform operates in physical pixels so
  // our WaylandSurface is the link that has to translate the units.  See also
  // comments in the implementation.
  //
  // Bounds that will be applied when the window state is finalized.  The window
  // may get several configuration events that update the pending bounds, and
  // only upon finalizing the state is the latest value stored as the current
  // bounds via |ApplyPendingBounds|.  Measured in DIP because updated in the
  // handler that receives DIP from Wayland.
  gfx::Rect pending_bounds_dip_;

  // Contains the current state of the window.
  PlatformWindowState state_;
  // Contains the previous state of the window.
  PlatformWindowState previous_state_;

  bool is_active_ = false;

  // Id of the chromium app passed through
  // PlatformWindowInitProperties::wm_class_class. This is used by Wayland
  // compositor to identify the app, unite it's windows into the same stack of
  // windows and find *.desktop file to set various preferences including icons.
  std::string app_id_;

  // Title of the ShellSurface.
  base::string16 window_title_;

  // Max and min sizes of the WaylandSurface window.
  base::Optional<gfx::Size> min_size_;
  base::Optional<gfx::Size> max_size_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
