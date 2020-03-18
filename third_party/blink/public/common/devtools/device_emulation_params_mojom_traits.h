// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVICE_EMULATION_PARAMS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVICE_EMULATION_PARAMS_MOJOM_TRAITS_H_

#include <utility>

#include "third_party/blink/public/common/devtools/web_device_emulation_params.h"
#include "third_party/blink/public/mojom/devtools/device_emulation_params.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::DeviceEmulationParamsDataView,
                 blink::WebDeviceEmulationParams> {
  static blink::mojom::ScreenPosition screen_position(
      const blink::WebDeviceEmulationParams& params) {
    return params.screen_position;
  }

  static gfx::Size screen_size(const blink::WebDeviceEmulationParams& params) {
    return params.screen_size;
  }

  static base::Optional<gfx::Point> view_position(
      const blink::WebDeviceEmulationParams& params) {
    return params.view_position;
  }

  static gfx::Size view_size(const blink::WebDeviceEmulationParams& params) {
    return params.view_size;
  }

  static float device_scale_factor(
      const blink::WebDeviceEmulationParams& params) {
    return params.device_scale_factor;
  }

  static float scale(const blink::WebDeviceEmulationParams& params) {
    return params.scale;
  }

  static gfx::PointF viewport_offset(
      const blink::WebDeviceEmulationParams& params) {
    return params.viewport_offset;
  }

  static float viewport_scale(const blink::WebDeviceEmulationParams& params) {
    return params.viewport_scale;
  }

  static blink::mojom::ScreenOrientationType screen_orientation_type(
      const blink::WebDeviceEmulationParams& params) {
    return params.screen_orientation_type;
  }

  static int screen_orientation_angle(
      const blink::WebDeviceEmulationParams& params) {
    return params.screen_orientation_angle;
  }

  static bool Read(blink::mojom::DeviceEmulationParamsDataView data,
                   blink::WebDeviceEmulationParams* params);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVICE_EMULATION_PARAMS_MOJOM_TRAITS_H_
