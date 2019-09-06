// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

// Contains Linux specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLinux
    : public DesktopWindowTreeHostPlatform {
 public:
  DesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostLinux() override;

 protected:
  // Adjusts |requested_size| to avoid the WM "feature" where setting the
  // window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  //
  // TODO(https://crbug.com/990756)): this method is mainly for X11
  // impl (Wayland does not need this workaround). Move this to X11Window
  // instead. We can't do it now as there are some methods that depend on this.
  gfx::Size AdjustSizeForDisplay(const gfx::Size& requested_size_in_pixels);

 private:
  // Overridden from display::DisplayObserver via aura::WindowTreeHost:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostLinux);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
