// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPHERICAL_HARMONICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPHERICAL_HARMONICS_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DOMPointReadOnly;

class XRSphericalHarmonics : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRSphericalHarmonics(
      const device::mojom::blink::XRSphericalHarmonics& spherical_harmonics);

  DOMPointReadOnly* orientation() const;
  DOMFloat32Array* coefficients() const;

  void Trace(Visitor* visitor) override;

 private:
  Member<DOMFloat32Array> coefficients_;
  Member<DOMPointReadOnly> orientation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPHERICAL_HARMONICS_H_
