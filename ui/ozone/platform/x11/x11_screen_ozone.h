// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display_list.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

// A PlatformScreen implementation for X11.
class X11ScreenOzone : public PlatformScreen, public PlatformEventDispatcher {
 public:
  X11ScreenOzone();
  ~X11ScreenOzone() override;

  // PlatformScreen:
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

 private:
  void AddDisplay(const display::Display& display, bool is_primary);
  void RemoveDisplay(const display::Display& display);
  void FetchDisplayList();
  gfx::Point GetCursorLocation() const;

  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver> observers_;

  XDisplay* const xdisplay_;
  XID x_root_window_;
  int64_t primary_display_index_ = 0;

  // XRandR version. MAJOR * 100 + MINOR. Zero if no xrandr is present.
  const int xrandr_version_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_ = 0;

  DISALLOW_COPY_AND_ASSIGN(X11ScreenOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
