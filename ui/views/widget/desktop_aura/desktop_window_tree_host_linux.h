// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

class SkPath;

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

  // This must be called before the window is created, because the visual cannot
  // be changed after. Useful for X11. Not in use for Wayland.
  void SetPendingXVisualId(int x_visual_id);

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;

  // PlatformWindowDelegateBase:
  void DispatchEvent(ui::Event* event) override;
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

  // Called back by compositor_observer_ if the latter is set.
  virtual void OnCompleteSwapWithNewSize(const gfx::Size& size);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // PlatformWindowDelegateLinux overrides:
  void OnWorkspaceChanged() override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterLinux> non_client_window_event_filter_;

  // X11 may set set a visual id for the system tray icon before the host is
  // initialized. This value will be passed down to PlatformWindow during
  // initialization of the host.
  base::Optional<int> pending_x_visual_id_;

  std::unique_ptr<CompositorObserver> compositor_observer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostLinux);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
