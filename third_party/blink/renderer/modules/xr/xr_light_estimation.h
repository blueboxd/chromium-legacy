// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_ESTIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_ESTIMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRLightProbe;
class XRReflectionProbe;

class XRLightEstimation : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRLightEstimation(
      const device::mojom::blink::XRLightEstimationData& data);

  XRLightProbe* lightProbe() const;
  XRReflectionProbe* reflectionProbe() const;

  void Trace(Visitor* visitor) override;

 private:
  Member<XRLightProbe> light_probe_;
  Member<XRReflectionProbe> reflection_probe_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_ESTIMATION_H_
