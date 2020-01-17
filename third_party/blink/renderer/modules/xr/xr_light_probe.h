// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRSphericalHarmonics;
class DOMPointReadOnly;

class XRLightProbe : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRLightProbe(const device::mojom::blink::XRLightProbe& light_probe);

  XRSphericalHarmonics* sphericalHarmonics() const;
  DOMPointReadOnly* mainLightDirection() const;
  DOMPointReadOnly* mainLightIntensity() const;

  void Trace(Visitor* visitor) override;

 private:
  Member<XRSphericalHarmonics> spherical_harmonics_;
  Member<DOMPointReadOnly> main_light_direction_;
  Member<DOMPointReadOnly> main_light_intensity_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_
