// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_

#include <stdint.h>
#include <wayland-server-protocol-core.h>

#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

struct wl_resource;

namespace exo {
namespace wayland {
class WaylandDisplayOutput;

class WaylandDisplayObserver : public display::DisplayObserver {
 public:
  class ScaleObserver : public base::CheckedObserver {
   public:
    ScaleObserver() {}

    virtual void OnDisplayScalesChanged(const display::Display& display) = 0;

   protected:
    ~ScaleObserver() override {}
  };

  WaylandDisplayObserver(WaylandDisplayOutput* output,
                         wl_resource* output_resource);
  ~WaylandDisplayObserver() override;
  void AddScaleObserver(ScaleObserver* scale_observer);
  bool HasScaleObserver(ScaleObserver* scale_observer) const;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void SendDisplayMetrics();
  // Returns the transform that a compositor will apply to a surface to
  // compensate for the rotation of an output device.
  wl_output_transform OutputTransform(display::Display::Rotation rotation);

  // Output.
  WaylandDisplayOutput* output_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  base::ObserverList<ScaleObserver> scale_observers_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayObserver);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
