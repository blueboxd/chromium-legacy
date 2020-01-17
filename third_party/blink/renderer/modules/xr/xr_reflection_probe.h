// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFLECTION_PROBE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFLECTION_PROBE_H_

#include "base/util/type_safety/pass_key.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRCubeMap;

class XRReflectionProbe : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRReflectionProbe(
      const device::mojom::blink::XRReflectionProbe& reflection_probe);

  XRCubeMap* cubeMap() const;

  void Trace(Visitor* visitor) override;

 private:
  Member<XRCubeMap> cube_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFLECTION_PROBE_H_
