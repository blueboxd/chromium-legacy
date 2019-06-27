// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane_space.h"

#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

XRPlaneSpace::XRPlaneSpace(XRSession* session, const XRPlane* plane)
    : XRSpace(session), plane_(plane) {}

std::unique_ptr<TransformationMatrix> XRPlaneSpace::GetTransformToMojoSpace() {
  auto mojo_to_plane = plane_->poseMatrix();

  if (!mojo_to_plane.IsInvertible()) {
    return nullptr;
  }

  return std::make_unique<TransformationMatrix>(mojo_to_plane.Inverse());
}

void XRPlaneSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(plane_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
