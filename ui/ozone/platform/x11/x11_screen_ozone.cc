// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"

namespace ui {

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

constexpr auto kDisplayListUpdateDelay = base::TimeDelta::FromMilliseconds(250);

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

X11ScreenOzone::X11ScreenOzone(X11WindowManagerOzone* window_manager)
    : window_manager_(window_manager),
      xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(GetXrandrVersion(xdisplay_)) {
  DCHECK(window_manager_);
}

X11ScreenOzone::~X11ScreenOzone() {
  if (xrandr_version_ >= kMinVersionXrandr &&
      X11EventSourceLibevent::GetInstance()) {
    X11EventSourceLibevent::GetInstance()->RemoveXEventDispatcher(this);
  }
}

void X11ScreenOzone::Init() {
  // Need at least xrandr version 1.3.
  if (xrandr_version_ >= kMinVersionXrandr) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    DCHECK(X11EventSourceLibevent::GetInstance());
    X11EventSourceLibevent::GetInstance()->AddXEventDispatcher(this);

    XRRSelectInput(xdisplay_, x_root_window_,
                   RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                       RRCrtcChangeNotifyMask);
  }
  FetchDisplayList();
}

const std::vector<display::Display>& X11ScreenOzone::GetAllDisplays() const {
  return displays_;
}

display::Display X11ScreenOzone::GetPrimaryDisplay() const {
  auto iter = displays_.begin();
  if (iter == displays_.end())
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
          displays_, gfx::ConvertRectToDIP(GetDeviceScaleFactor(), match_rect));
  return matching_display ? *matching_display : GetPrimaryDisplay();
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

// TODO(nickdiego): Factor event dispatching and display fetching so that it
// can be shared between ozone and non-ozone code paths.
bool X11ScreenOzone::DispatchXEvent(XEvent* xev) {
  DCHECK(xev);
  int ev_type = xev->type - xrandr_event_base_;
  if (ev_type == RRScreenChangeNotify) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(xev);
    return true;
  }
  if (ev_type == RRNotify ||
      (xev->type == PropertyNotify &&
       xev->xproperty.atom == gfx::GetAtom("_NET_WORKAREA"))) {
    RestartDelayedUpdateDisplayListTask();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// X11ScreenOzone, private:

void X11ScreenOzone::SetDisplayList(std::vector<display::Display> displays) {
  displays_ = std::move(displays);
  gfx::SetFontRenderParamsDeviceScaleFactor(
      GetPrimaryDisplay().device_scale_factor());
}

// Talks to xrandr to get the information of the outputs for a screen and
// updates display::Display list. The minimum required version of xrandr is
// 1.3.
void X11ScreenOzone::FetchDisplayList() {
  std::vector<display::Display> displays;
  float scale = GetDeviceScaleFactor();
  if (xrandr_version_ >= kMinVersionXrandr) {
    displays = BuildDisplaysFromXRandRInfo(xrandr_version_, scale,
                                           &primary_display_index_);
  } else {
    displays = GetFallbackDisplayList(scale);
  }
  SetDisplayList(std::move(displays));
}

void X11ScreenOzone::UpdateDisplayList() {
  std::vector<display::Display> old_displays = displays_;
  FetchDisplayList();
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

void X11ScreenOzone::RestartDelayedUpdateDisplayListTask() {
  delayed_update_task_.Reset(base::BindOnce(&X11ScreenOzone::UpdateDisplayList,
                                            base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, delayed_update_task_.callback(), kDisplayListUpdateDelay);
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
