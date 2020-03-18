// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/devtools/device_emulation_params_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::DeviceEmulationParamsDataView,
                  blink::WebDeviceEmulationParams>::
    Read(blink::mojom::DeviceEmulationParamsDataView data,
         blink::WebDeviceEmulationParams* params) {
  if (!data.ReadScreenPosition(&params->screen_position))
    return false;
  if (!data.ReadScreenSize(&params->screen_size))
    return false;
  if (!data.ReadViewPosition(&params->view_position))
    return false;
  if (!data.ReadViewSize(&params->view_size))
    return false;
  params->device_scale_factor = data.device_scale_factor();
  params->scale = data.scale();
  if (!data.ReadViewportOffset(&params->viewport_offset))
    return false;
  params->viewport_scale = data.viewport_scale();
  if (!data.ReadScreenOrientationType(&params->screen_orientation_type))
    return false;
  params->screen_orientation_angle = data.screen_orientation_angle();
  return true;
}

}  // namespace mojo
