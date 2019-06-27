// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SPACE_H_

#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

class TransformationMatrix;
class XRPlane;
class XRSession;

class XRPlaneSpace : public XRSpace {
 public:
  explicit XRPlaneSpace(XRSession* session, const XRPlane* plane);

  std::unique_ptr<TransformationMatrix> GetTransformToMojoSpace() override;

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<const XRPlane> plane_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SPACE_H_
