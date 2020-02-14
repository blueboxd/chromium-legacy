// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_light_estimation_state_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_plane_detection_state_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_world_tracking_state_init.h"
#include "third_party/blink/renderer/modules/xr/xr_light_estimation_state.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"

namespace blink {

XRWorldTrackingState::XRWorldTrackingState(
    XRWorldTrackingStateInit* world_tracking_state_init) {
  if (world_tracking_state_init &&
      world_tracking_state_init->hasPlaneDetectionState()) {
    plane_detection_state_ = MakeGarbageCollected<XRPlaneDetectionState>(
        world_tracking_state_init->planeDetectionState());
  } else {
    plane_detection_state_ =
        MakeGarbageCollected<XRPlaneDetectionState>(nullptr);
  }

  if (world_tracking_state_init &&
      world_tracking_state_init->hasLightEstimationState()) {
    light_estimation_state_ = MakeGarbageCollected<XRLightEstimationState>(
        world_tracking_state_init->lightEstimationState());
  } else {
    light_estimation_state_ =
        MakeGarbageCollected<XRLightEstimationState>(nullptr);
  }
}

void XRWorldTrackingState::Trace(Visitor* visitor) {
  visitor->Trace(plane_detection_state_);
  visitor->Trace(light_estimation_state_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
