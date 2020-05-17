// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XRCubeMap;
class XRLightEstimate;
class XRSession;

class XRLightProbe : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRLightProbe(XRSession* session);

  XRSession* session() const { return session_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reflectionchange, kReflectionchange)

  void ProcessLightEstimationData(
      const device::mojom::blink::XRLightEstimationData* data,
      double timestamp);

  XRLightEstimate* getLightEstimate() { return light_estimate_; }
  XRCubeMap* getReflectionCubeMap() { return cube_map_.get(); }

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void Trace(Visitor* visitor) const override;

 private:
  Member<XRSession> session_;
  Member<XRLightEstimate> light_estimate_;

  double last_reflection_change_ = 0.0;
  std::unique_ptr<XRCubeMap> cube_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LIGHT_PROBE_H_
