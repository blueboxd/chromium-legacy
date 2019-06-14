// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

float GetDeviceScaleFactor() {
  float device_scale_factor = 1.0f;
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  // Get device scale factor using scale factor and resolution like
  // 'GtkUi::GetRawDeviceScaleFactor'.
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  return device_scale_factor;
}

gfx::Point PixelToDIPPoint(const gfx::Point& pixel_point) {
  return gfx::ConvertPointToDIP(GetDeviceScaleFactor(), pixel_point);
}

}  // namespace

X11ScreenOzone::X11ScreenOzone()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(GetXrandrVersion(xdisplay_)) {
  FetchDisplayList();
}

X11ScreenOzone::~X11ScreenOzone() {
  if (xrandr_version_ >= kMinVersionXrandr &&
      PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }
}

const std::vector<display::Display>& X11ScreenOzone::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display X11ScreenOzone::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return display::Display::GetDefaultDisplay();
  return *iter;
}

display::Display X11ScreenOzone::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

gfx::Point X11ScreenOzone::GetCursorScreenPoint() const {
  if (ui::X11EventSource::HasInstance()) {
    base::Optional<gfx::Point> point =
        ui::X11EventSource::GetInstance()
            ->GetRootCursorLocationFromCurrentEvent();
    if (point)
      return PixelToDIPPoint(point.value());
  }
  return PixelToDIPPoint(GetCursorLocation());
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

bool X11ScreenOzone::CanDispatchEvent(const ui::PlatformEvent& event) {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

uint32_t X11ScreenOzone::DispatchEvent(const ui::PlatformEvent& event) {
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::POST_DISPATCH_NONE;
}

void X11ScreenOzone::AddDisplay(const display::Display& display,
                                bool is_primary) {
  display_list_.AddDisplay(
      display, is_primary ? display::DisplayList::Type::PRIMARY
                          : display::DisplayList::Type::NOT_PRIMARY);

  if (is_primary) {
    gfx::SetFontRenderParamsDeviceScaleFactor(
        GetPrimaryDisplay().device_scale_factor());
  }
}

void X11ScreenOzone::RemoveDisplay(const display::Display& display) {
  display_list_.RemoveDisplay(display.id());
}

// Talks to xrandr to get the information of the outputs for a screen and
// updates display::Display list. The minimum required version of xrandr is
// 1.3.
void X11ScreenOzone::FetchDisplayList() {
  float scale = GetDeviceScaleFactor();
  std::vector<display::Display> displays;
  // Need at least xrandr version 1.3.
  if (xrandr_version_ >= kMinVersionXrandr) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    if (PlatformEventSource::GetInstance())
      PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    XRRSelectInput(xdisplay_, x_root_window_,
                   RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                       RRCrtcChangeNotifyMask);

    displays = BuildDisplaysFromXRandRInfo(xrandr_version_, scale,
                                           &primary_display_index_);
  } else {
    displays = GetFallbackDisplayList(scale);
  }
  for (auto& display : displays)
    AddDisplay(display, display.id() == primary_display_index_);
}

gfx::Point X11ScreenOzone::GetCursorLocation() const {
  ::Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(xdisplay_, x_root_window_, &root, &child, &root_x, &root_y,
                &win_x, &win_y, &mask);

  return gfx::Point(root_x, root_y);
}

}  // namespace ui
