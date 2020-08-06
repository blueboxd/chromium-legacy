// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"

#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"

namespace blink {

XRWorldTrackingState::XRWorldTrackingState(bool plane_detection_enabled)
    : plane_detection_state_(MakeGarbageCollected<XRPlaneDetectionState>(
          plane_detection_enabled)) {}

void XRWorldTrackingState::Trace(Visitor* visitor) const {
  visitor->Trace(plane_detection_state_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
