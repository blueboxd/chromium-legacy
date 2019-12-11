// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_

#include <memory>

#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

namespace ui {

class XDGSurfaceWrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Popup wrapper for xdg-shell stable and xdg-shell-unstable-v6
class XDGPopupWrapperImpl : public ShellPopupWrapper {
 public:
  XDGPopupWrapperImpl(std::unique_ptr<XDGSurfaceWrapperImpl> surface,
                      WaylandWindow* wayland_window);
  ~XDGPopupWrapperImpl() override;

  // XDGPopupWrapper:
  bool Initialize(WaylandConnection* connection,
                  const gfx::Rect& bounds) override;

 private:
  bool InitializeStable(WaylandConnection* connection,
                        const gfx::Rect& bounds,
                        XDGSurfaceWrapperImpl* parent_xdg_surface);
  struct xdg_positioner* CreatePositionerStable(WaylandConnection* connection,
                                                WaylandWindow* parent_window,
                                                const gfx::Rect& bounds);

  bool InitializeV6(WaylandConnection* connection,
                    const gfx::Rect& bounds,
                    XDGSurfaceWrapperImpl* parent_xdg_surface);
  struct zxdg_positioner_v6* CreatePositionerV6(WaylandConnection* connection,
                                                WaylandWindow* parent_window,
                                                const gfx::Rect& bounds);

  // xdg_popup_listener
  static void Configure(void* data,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height);
  static void ConfigureStable(void* data,
                              struct xdg_popup* xdg_popup,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height);
  static void ConfigureV6(void* data,
                          struct zxdg_popup_v6* zxdg_popup_v6,
                          int32_t x,
                          int32_t y,
                          int32_t width,
                          int32_t height);
  static void PopupDone(void* data);
  static void PopupDoneStable(void* data, struct xdg_popup* xdg_popup);
  static void PopupDoneV6(void* data, struct zxdg_popup_v6* zxdg_popup_v6);

  XDGSurfaceWrapperImpl* xdg_surface();

  // Non-owned WaylandWindow that uses this popup.
  WaylandWindow* const wayland_window_;

  // Ground surface for this popup.
  std::unique_ptr<XDGSurfaceWrapperImpl> xdg_surface_;

  // XDG Shell Stable object.
  wl::Object<xdg_popup> xdg_popup_;
  // XDG Shell V6 object.
  wl::Object<zxdg_popup_v6> zxdg_popup_v6_;

  DISALLOW_COPY_AND_ASSIGN(XDGPopupWrapperImpl);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
