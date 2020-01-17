// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reflection_probe.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"

namespace blink {

XRReflectionProbe::XRReflectionProbe(
    const device::mojom::blink::XRReflectionProbe& reflection_probe) {
  cube_map_ = MakeGarbageCollected<XRCubeMap>(*reflection_probe.cube_map);
}

XRCubeMap* XRReflectionProbe::cubeMap() const {
  return cube_map_.Get();
}

void XRReflectionProbe::Trace(Visitor* visitor) {
  visitor->Trace(cube_map_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
