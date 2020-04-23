// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_light_estimate.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRLightProbe::XRLightProbe(XRSession* session) : session_(session) {}

void XRLightProbe::ProcessLightEstimationData(
    const device::mojom::blink::XRLightEstimationData* data,
    double timestamp) {
  if (data) {
    light_estimate_ = MakeGarbageCollected<XRLightEstimate>(*data->light_probe);
  } else {
    light_estimate_ = nullptr;
  }
}

void XRLightProbe::Trace(Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(light_estimate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
