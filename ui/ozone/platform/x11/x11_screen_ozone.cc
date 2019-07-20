// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/x/x11.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"

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

// ui::EnumerateTopLevelWindows API is used here to retrieve the x11 window
// stack, so that windows are checked in descending z-order, covering window
// overlapping cases gracefully.
// TODO(nickdiego): Consider refactoring ui::EnumerateTopLevelWindows to use
// lambda/callback instead of Delegate interface.
class LocalProcessWindowFinder : public EnumerateWindowsDelegate {
 public:
  explicit LocalProcessWindowFinder(X11WindowManagerOzone* window_manager);
  ~LocalProcessWindowFinder() override = default;

  X11WindowOzone* FindWindowAt(const gfx::Point& screen_point_in_pixels);

 private:
  // ui::EnumerateWindowsDelegate
  bool ShouldStopIterating(XID xid) override;

  // Returns true if |window| is visible and contains the
  // |screen_point_in_pixels_| within its bounds, even if custom shape is used.
  bool MatchWindow(X11WindowOzone* window) const;

  X11WindowManagerOzone* const window_manager_;
  X11WindowOzone* window_found_ = nullptr;
  gfx::Point screen_point_in_pixels_;
};

LocalProcessWindowFinder::LocalProcessWindowFinder(
    X11WindowManagerOzone* window_manager)
    : window_manager_(window_manager) {
  DCHECK(window_manager_);
}

X11WindowOzone* LocalProcessWindowFinder::FindWindowAt(
    const gfx::Point& screen_point_in_pixels) {
  screen_point_in_pixels_ = screen_point_in_pixels;
  ui::EnumerateTopLevelWindows(this);
  return window_found_;
}

bool LocalProcessWindowFinder::ShouldStopIterating(XID xid) {
  X11WindowOzone* window = window_manager_->GetWindow(xid);
  if (!window || !MatchWindow(window))
    return false;

  window_found_ = window;
  return true;
}

bool LocalProcessWindowFinder::MatchWindow(X11WindowOzone* window) const {
  DCHECK(window);

  if (!window->IsVisible())
    return false;

  gfx::Rect window_bounds = window->GetOutterBounds();
  if (!window_bounds.Contains(screen_point_in_pixels_))
    return false;

  ::Region shape = window->GetShape();
  if (!shape)
    return true;

  gfx::Point window_point(screen_point_in_pixels_);
  window_point.Offset(-window_bounds.origin().x(), -window_bounds.origin().y());
  return XPointInRegion(shape, window_point.x(), window_point.y()) == x11::True;
}

}  // namespace

X11ScreenOzone::X11ScreenOzone(X11WindowManagerOzone* wm, bool fetch)
    : window_manager_(wm),
      xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(GetXrandrVersion(xdisplay_)) {
  DCHECK(window_manager_);

  // TODO(nickdiego): Factor this out from ctor
  if (fetch)
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
  if (widget == gfx::kNullAcceleratedWidget)
    return GetPrimaryDisplay();

  X11WindowOzone* window = window_manager_->GetWindow(widget);
  return window ? GetDisplayMatching(window->GetBounds()) : GetPrimaryDisplay();
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
  LocalProcessWindowFinder finder(window_manager_);
  X11WindowOzone* window = finder.FindWindowAt(point);
  return window ? window->widget() : gfx::kNullAcceleratedWidget;
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1)
    return GetPrimaryDisplay();
  return *display::FindDisplayNearestPoint(displays, point);
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* matching_display =
      display::FindDisplayWithBiggestIntersection(
          display_list_.displays(),
          gfx::ConvertRectToDIP(GetDeviceScaleFactor(), match_rect));
  return matching_display ? *matching_display : GetPrimaryDisplay();
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

////////////////////////////////////////////////////////////////////////////////
// X11ScreenOzone, private:

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
