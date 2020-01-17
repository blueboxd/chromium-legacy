// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_spherical_harmonics.h"

namespace blink {

XRLightProbe::XRLightProbe(
    const device::mojom::blink::XRLightProbe& light_probe) {
  spherical_harmonics_ = MakeGarbageCollected<XRSphericalHarmonics>(
      *light_probe.spherical_harmonics);

  main_light_direction_ =
      DOMPointReadOnly::Create(light_probe.main_light_direction.x(),
                               light_probe.main_light_direction.y(),
                               light_probe.main_light_direction.z(), 0);
  main_light_intensity_ =
      DOMPointReadOnly::Create(light_probe.main_light_intensity.red(),
                               light_probe.main_light_intensity.green(),
                               light_probe.main_light_intensity.blue(), 1);
}

XRSphericalHarmonics* XRLightProbe::sphericalHarmonics() const {
  return spherical_harmonics_.Get();
}

DOMPointReadOnly* XRLightProbe::mainLightDirection() const {
  return main_light_direction_.Get();
}

DOMPointReadOnly* XRLightProbe::mainLightIntensity() const {
  return main_light_intensity_.Get();
}

void XRLightProbe::Trace(Visitor* visitor) {
  visitor->Trace(spherical_harmonics_);
  visitor->Trace(main_light_direction_);
  visitor->Trace(main_light_intensity_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
