// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_spherical_harmonics.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"

namespace blink {

XRSphericalHarmonics::XRSphericalHarmonics(
    const device::mojom::blink::XRSphericalHarmonics& spherical_harmonics) {
  DCHECK_EQ(spherical_harmonics.coefficients.size(), 9u);

  coefficients_ = DOMFloat32Array::Create(
      spherical_harmonics.coefficients.data()->components,
      spherical_harmonics.coefficients.size() *
          device::RgbTupleF32::kNumComponents);

  orientation_ = DOMPointReadOnly::Create(0, 0, 0, 1);
}

DOMPointReadOnly* XRSphericalHarmonics::orientation() const {
  return orientation_.Get();
}

DOMFloat32Array* XRSphericalHarmonics::coefficients() const {
  return coefficients_.Get();
}

void XRSphericalHarmonics::Trace(Visitor* visitor) {
  visitor->Trace(coefficients_);
  visitor->Trace(orientation_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
