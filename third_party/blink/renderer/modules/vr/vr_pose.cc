// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_pose.h"

namespace blink {

namespace {

DOMFloat32Array* QuaternionToFloat32Array(
    const base::Optional<gfx::Quaternion>& quat) {
  if (!quat)
    return nullptr;

  auto* dom_array = DOMFloat32Array::Create(4);
  float* data = dom_array->Data();
  data[0] = quat->x();
  data[1] = quat->y();
  data[2] = quat->z();
  data[3] = quat->w();

  return dom_array;
}

DOMFloat32Array* Vector3dFToFloat32Array(
    const base::Optional<gfx::Vector3dF>& vec) {
  if (!vec)
    return nullptr;

  auto* dom_array = DOMFloat32Array::Create(3);
  float* data = dom_array->Data();
  data[0] = vec->x();
  data[1] = vec->y();
  data[2] = vec->z();

  return dom_array;
}

DOMFloat32Array* WebPoint3DToFloat32Array(
    const base::Optional<WebFloatPoint3D>& p) {
  if (!p)
    return nullptr;

  auto* dom_array = DOMFloat32Array::Create(3);
  float* data = dom_array->Data();
  data[0] = p->x;
  data[1] = p->y;
  data[2] = p->z;

  return dom_array;
}

}  // namespace

VRPose::VRPose() = default;

void VRPose::SetPose(const device::mojom::blink::VRPosePtr& state) {
  if (state.is_null())
    return;

  orientation_ = QuaternionToFloat32Array(state->orientation);
  position_ = WebPoint3DToFloat32Array(state->position);
  angular_velocity_ = Vector3dFToFloat32Array(state->angular_velocity);
  linear_velocity_ = Vector3dFToFloat32Array(state->linear_velocity);
  angular_acceleration_ = Vector3dFToFloat32Array(state->angular_acceleration);
  linear_acceleration_ = Vector3dFToFloat32Array(state->linear_acceleration);
}

void VRPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(orientation_);
  visitor->Trace(position_);
  visitor->Trace(angular_velocity_);
  visitor->Trace(linear_velocity_);
  visitor->Trace(angular_acceleration_);
  visitor->Trace(linear_acceleration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
