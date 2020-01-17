// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_light_estimation.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_reflection_probe.h"

namespace blink {

XRLightEstimation::XRLightEstimation(
    const device::mojom::blink::XRLightEstimationData& data) {
  if (data.light_probe) {
    light_probe_ = MakeGarbageCollected<XRLightProbe>(*data.light_probe);
  }
  if (data.reflection_probe) {
    reflection_probe_ =
        MakeGarbageCollected<XRReflectionProbe>(*data.reflection_probe);
  }
}

XRLightProbe* XRLightEstimation::lightProbe() const {
  return light_probe_.Get();
}

XRReflectionProbe* XRLightEstimation::reflectionProbe() const {
  return reflection_probe_.Get();
}

void XRLightEstimation::Trace(Visitor* visitor) {
  visitor->Trace(light_probe_);
  visitor->Trace(reflection_probe_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
