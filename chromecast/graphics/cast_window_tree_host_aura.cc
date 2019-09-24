// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_tree_host_aura.h"

#include "ui/aura/null_window_targeter.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace chromecast {

CastWindowTreeHostAura::CastWindowTreeHostAura(
    bool enable_input,
    ui::PlatformWindowInitProperties properties,
    bool use_external_frame_control)
    : WindowTreeHostPlatform(std::move(properties),
                             nullptr,
                             nullptr,
                             use_external_frame_control),
      enable_input_(enable_input) {
  if (!enable_input)
    window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());
}

CastWindowTreeHostAura::~CastWindowTreeHostAura() {}

void CastWindowTreeHostAura::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
}

gfx::Rect CastWindowTreeHostAura::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& host_size_in_pixels) const {
  gfx::RectF new_bounds(WindowTreeHost::GetTransformedRootWindowBoundsInPixels(
      host_size_in_pixels));
  new_bounds.set_origin(gfx::PointF());
  return gfx::ToEnclosingRect(new_bounds);
}

}  // namespace chromecast
