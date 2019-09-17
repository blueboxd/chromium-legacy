// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

class WindowEventFilterLinux;

// Contains Linux specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLinux
    : public DesktopWindowTreeHostPlatform {
 public:
  DesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostLinux() override;

 protected:
  // Overridden from DesktopWindowTreeHost:
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;

  // PlatformWindowDelegateBase:
  void OnClosed() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostLinuxTest, HitTest);

  // Overridden from display::DisplayObserver via aura::WindowTreeHost:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

  void AddNonClientEventFilter();
  void RemoveNonClientEventFilter();

  // A handler for events intended for non client area.
  std::unique_ptr<WindowEventFilterLinux> non_client_window_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostLinux);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
