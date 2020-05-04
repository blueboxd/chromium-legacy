// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_

#include <memory>
#include <vector>

#include "base/scoped_observer.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/views/linux_ui/device_scale_factor_observer.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_export.h"

namespace views {
class DesktopScreenX11Test;

namespace test {
class DesktopScreenX11TestApi;
}

// Screen implementation that talks to XRandR
class VIEWS_EXPORT DesktopScreenX11 : public display::Screen,
                                      public ui::XEventDispatcher,
                                      public ui::XDisplayManager::Delegate,
                                      public DeviceScaleFactorObserver {
 public:
  DesktopScreenX11();
  ~DesktopScreenX11() override;

  // Fetches display list using XRandR. Must be called explicitly as actual
  // fetching might not be desirable in some scenarios (e.g: unit tests)
  void Init();

  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  std::string GetCurrentWorkspace() override;

  // ui::XEventDispatcher:
  bool DispatchXEvent(XEvent* event) override;

  // DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override;

  static void UpdateDeviceScaleFactorForTest();

 private:
  friend class DesktopScreenX11Test;
  friend class test::DesktopScreenX11TestApi;

  // ui::XDisplayManager::Delegate:
  void OnXDisplayListUpdated() override;
  float GetXDisplayScaleFactor() const override;

  std::unique_ptr<ui::XDisplayManager> x11_display_manager_ =
      std::make_unique<ui::XDisplayManager>(this);
  ScopedObserver<LinuxUI,
                 DeviceScaleFactorObserver,
                 &LinuxUI::AddDeviceScaleFactorObserver,
                 &LinuxUI::RemoveDeviceScaleFactorObserver>
      display_scale_factor_observer_{this};
  ScopedObserver<ui::X11EventSource,
                 XEventDispatcher,
                 &ui::X11EventSource::AddXEventDispatcher,
                 &ui::X11EventSource::RemoveXEventDispatcher>
      event_source_observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
