// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

namespace blink {
namespace {

// Bounds must be a valid polygon (at least 3 vertices).
constexpr wtf_size_t kMinimumNumberOfBoundVertices = 3;

float RoundCm(float val) {
  // Float round will only round to the nearest whole number. In order to get
  // two decimal points of precision, we need to move the decimal out then
  // back.
  return std::round(val * 100) / 100;
}

Member<DOMPointReadOnly> RoundedDOMPoint(const FloatPoint3D& val) {
  return DOMPointReadOnly::Create(RoundCm(val.X()), RoundCm(val.Y()),
                                  RoundCm(val.Z()), 1.0);
}
}  // anonymous namespace

XRBoundedReferenceSpace::XRBoundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session, Type::kTypeBoundedFloor) {}

XRBoundedReferenceSpace::XRBoundedReferenceSpace(
    XRSession* session,
    XRRigidTransform* origin_offset)
    : XRReferenceSpace(session, origin_offset, Type::kTypeBoundedFloor) {}

XRBoundedReferenceSpace::~XRBoundedReferenceSpace() = default;

void XRBoundedReferenceSpace::EnsureUpdated() {
  // Check first to see if the stage parameters have updated since the last
  // call. We only need to update the transform and bounds if it has.
  if (stage_parameters_id_ == session()->StageParametersId())
    return;

  stage_parameters_id_ = session()->StageParametersId();

  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stage_parameters) {
    // Use the transform given by xrDisplayInfo's stage_parameters if available.
    bounded_native_from_mojo_ = std::make_unique<TransformationMatrix>(
        display_info->stage_parameters->standing_transform.matrix());

    // In order to ensure that the bounds continue to line up with the user's
    // physical environment we need to transform them from native to offset.
    // Bounds are provided in our native coordinate space.
    // TODO(https://crbug.com/1008466): move originOffset to separate class? If
    // yes, that class would need to apply a transform in the boundsGeometry
    // accessor.
    TransformationMatrix offset_from_native = OffsetFromNativeMatrix();

    // We may not have bounds if we've lost tracking after being created.
    // Whether we have them or not, we need to clear the existing bounds.
    offset_bounds_geometry_.clear();
    if (display_info->stage_parameters->bounds &&
        display_info->stage_parameters->bounds->size() >=
            kMinimumNumberOfBoundVertices) {
      for (const auto& bound : *(display_info->stage_parameters->bounds)) {
        FloatPoint3D p = offset_from_native.MapPoint(
            FloatPoint3D(bound.x(), 0.0, bound.z()));
        offset_bounds_geometry_.push_back(RoundedDOMPoint(p));
      }
    }
  } else {
    // If stage parameters aren't available set the transform to null, which
    // will subsequently cause this reference space to return null poses.
    bounded_native_from_mojo_.reset();
    offset_bounds_geometry_.clear();
  }

  DispatchEvent(*XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
}

base::Optional<TransformationMatrix> XRBoundedReferenceSpace::NativeFromMojo() {
  EnsureUpdated();

  if (!bounded_native_from_mojo_)
    return base::nullopt;

  return *bounded_native_from_mojo_;
}

HeapVector<Member<DOMPointReadOnly>> XRBoundedReferenceSpace::boundsGeometry() {
  EnsureUpdated();
  return offset_bounds_geometry_;
}

base::Optional<XRNativeOriginInformation>
XRBoundedReferenceSpace::NativeOrigin() const {
  return XRNativeOriginInformation::Create(this);
}

void XRBoundedReferenceSpace::Trace(Visitor* visitor) {
  visitor->Trace(offset_bounds_geometry_);
  XRReferenceSpace::Trace(visitor);
}

void XRBoundedReferenceSpace::OnReset() {
  // Anything that would cause an external source to try to tell us that we've
  // been reset should have also updated the stage_parameters, and thus caused
  // us to reset via that mechanism instead.
}

XRBoundedReferenceSpace* XRBoundedReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) {
  return MakeGarbageCollected<XRBoundedReferenceSpace>(this->session(),
                                                       origin_offset);
}

}  // namespace blink
