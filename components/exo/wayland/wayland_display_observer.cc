// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_observer.h"

#include <wayland-server-core.h>

#include <string>

#include "components/exo/wm_helper.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

namespace exo {
namespace wayland {

WaylandDisplayObserver::WaylandDisplayObserver(int64_t id,
                                               wl_resource* output_resource)
    : id_(id), output_resource_(output_resource) {
  display::Screen::GetScreen()->AddObserver(this);
  SendDisplayMetrics();
}

WaylandDisplayObserver::~WaylandDisplayObserver() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void WaylandDisplayObserver::SetScaleObserver(
    base::WeakPtr<ScaleObserver> scale_observer) {
  scale_observer_ = scale_observer;
  SendDisplayMetrics();
}

bool WaylandDisplayObserver::HasScaleObserver() const {
  return !!scale_observer_;
}

void WaylandDisplayObserver::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (id_ != display.id())
    return;

  // There is no need to check DISPLAY_METRIC_PRIMARY because when primary
  // changes, bounds always changes. (new primary should have had non
  // 0,0 origin).
  // Only exception is when switching to newly connected primary with
  // the same bounds. This happens whenyou're in docked mode, suspend,
  // unplug the dislpay, then resume to the internal display which has
  // the same resolution. Since metrics does not change, there is no need
  // to notify clients.
  if (changed_metrics &
      (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
       DISPLAY_METRIC_ROTATION)) {
    SendDisplayMetrics();
  }
}

void WaylandDisplayObserver::SendDisplayMetrics() {
  display::Display display;
  bool rv =
      display::Screen::GetScreen()->GetDisplayWithDisplayId(id_, &display);
  DCHECK(rv);

  const display::ManagedDisplayInfo& info =
      WMHelper::GetInstance()->GetDisplayInfo(display.id());

  const float kInchInMm = 25.4f;
  const char* kUnknown = "unknown";

  const std::string& make = info.manufacturer_id();
  const std::string& model = info.product_id();

  gfx::Rect bounds = info.bounds_in_native();

  // |origin| is used in wayland service to identify the workspace
  // the pixel size will be applied.
  gfx::Point origin = display.bounds().origin();
  // Don't use ManagedDisplayInfo.bound_in_native() because it
  // has raw information before overscan, rotation applied.
  gfx::Size size_in_pixel = display.GetSizeInPixel();

  // Use panel_rotation otherwise some X apps will refuse to take events from
  // outside the "visible" region.
  wl_output_send_geometry(
      output_resource_, origin.x(), origin.y(),
      static_cast<int>(kInchInMm * size_in_pixel.width() / info.device_dpi()),
      static_cast<int>(kInchInMm * size_in_pixel.height() / info.device_dpi()),
      WL_OUTPUT_SUBPIXEL_UNKNOWN, make.empty() ? kUnknown : make.c_str(),
      model.empty() ? kUnknown : model.c_str(),
      OutputTransform(display.panel_rotation()));

  if (wl_resource_get_version(output_resource_) >=
      WL_OUTPUT_SCALE_SINCE_VERSION) {
    wl_output_send_scale(output_resource_, display.device_scale_factor());
  }

  // TODO(reveman): Send real list of modes.
  wl_output_send_mode(output_resource_,
                      WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      bounds.width(), bounds.height(), static_cast<int>(60000));

  if (HasScaleObserver())
    scale_observer_->OnDisplayScalesChanged(display);

  if (wl_resource_get_version(output_resource_) >=
      WL_OUTPUT_DONE_SINCE_VERSION) {
    wl_output_send_done(output_resource_);
  }

  wl_client_flush(wl_resource_get_client(output_resource_));
}

wl_output_transform WaylandDisplayObserver::OutputTransform(
    display::Display::Rotation rotation) {
  // Note: |rotation| describes the counter clockwise rotation that a
  // display's output is currently adjusted for, which is the inverse
  // of what we need to return.
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_90;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

}  // namespace wayland
}  // namespace exo
