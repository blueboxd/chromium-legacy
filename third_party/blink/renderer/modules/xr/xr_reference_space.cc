// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeightMeters = 1.6;

XRReferenceSpace::Type XRReferenceSpace::StringToReferenceSpaceType(
    const String& reference_space_type) {
  if (reference_space_type == "viewer") {
    return XRReferenceSpace::Type::kTypeViewer;
  } else if (reference_space_type == "local") {
    return XRReferenceSpace::Type::kTypeLocal;
  } else if (reference_space_type == "local-floor") {
    return XRReferenceSpace::Type::kTypeLocalFloor;
  } else if (reference_space_type == "bounded-floor") {
    return XRReferenceSpace::Type::kTypeBoundedFloor;
  } else if (reference_space_type == "unbounded") {
    return XRReferenceSpace::Type::kTypeUnbounded;
  }
  NOTREACHED();
  return Type::kTypeViewer;
}

// origin offset starts as identity transform
XRReferenceSpace::XRReferenceSpace(XRSession* session, Type type)
    : XRReferenceSpace(session,
                       MakeGarbageCollected<XRRigidTransform>(nullptr, nullptr),
                       type) {}

XRReferenceSpace::XRReferenceSpace(XRSession* session,
                                   XRRigidTransform* origin_offset,
                                   Type type)
    : XRSpace(session), origin_offset_(origin_offset), type_(type) {}

XRReferenceSpace::~XRReferenceSpace() = default;

XRPose* XRReferenceSpace::getPose(
    XRSpace* other_space,
    const TransformationMatrix* base_pose_matrix) {
  if (type_ == Type::kTypeViewer) {
    std::unique_ptr<TransformationMatrix> viewer_pose_matrix =
        other_space->GetViewerPoseMatrix(base_pose_matrix);
    if (!viewer_pose_matrix) {
      return nullptr;
    }
    return MakeGarbageCollected<XRPose>(*viewer_pose_matrix,
                                        session()->EmulatedPosition());
  } else {
    return XRSpace::getPose(other_space, base_pose_matrix);
  }
}

void XRReferenceSpace::UpdateFloorLevelTransform() {
  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stage_parameters) {
    // Use the transform given by xrDisplayInfo's stage_parameters if available.
    floor_level_transform_ = std::make_unique<TransformationMatrix>(
        display_info->stage_parameters->standing_transform.matrix());
  } else {
    // Otherwise, create a transform based on the default emulated height.
    floor_level_transform_ = std::make_unique<TransformationMatrix>();
    floor_level_transform_->Translate3d(0, kDefaultEmulationHeightMeters, 0);
  }

  display_info_id_ = session()->DisplayInfoPtrId();
}

// Returns a default pose if no base pose is available. Only applicable to
// viewer reference spaces.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::DefaultPose() {
  // A viewer reference space always returns an identity matrix.
  return type_ == Type::kTypeViewer ? std::make_unique<TransformationMatrix>()
                                    : nullptr;
}

// Transforms a given pose from a "base" reference space used by the XR
// service to the space represenced by this reference space.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  switch (type_) {
    case Type::kTypeLocal:
      // Currently all base poses are 'local' poses, so return directly.
      return std::make_unique<TransformationMatrix>(base_pose);
      break;
    case Type::kTypeLocalFloor:
      // Currently all base poses are 'local' space, so use of 'local-floor'
      // reference spaces requires adjustment. Ideally the service will
      // eventually provide poses in the requested space directly, avoiding the
      // need to do additional transformation here.

      // Check first to see if the xrDisplayInfo has updated since the last
      // call. If so, update the floor-level transform.
      if (display_info_id_ != session()->DisplayInfoPtrId())
        UpdateFloorLevelTransform();

      // Apply the floor-level transform to the base pose.
      if (floor_level_transform_) {
        auto pose =
            std::make_unique<TransformationMatrix>(*floor_level_transform_);
        pose->Multiply(base_pose);
        return pose;
      }
      break;
    case Type::kTypeViewer:
      // Always return the default pose because we will only get here for an
      // "viewer" reference space.
      return DefaultPose();
    case Type::kTypeUnbounded:
      // For now we assume that poses returned by systems that support unbounded
      // reference spaces are already in the correct space.
      return std::make_unique<TransformationMatrix>(base_pose);
    case Type::kTypeBoundedFloor:
      break;
  }

  return nullptr;
}

// Serves the same purpose as TransformBasePose, but for input poses. Needs to
// know the head pose so that cases like the viewer frame of reference can
// properly adjust the input's relative position.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  return TransformBasePose(base_input_pose);
}

std::unique_ptr<TransformationMatrix>
XRReferenceSpace::GetTransformToMojoSpace() {
  // XRReferenceSpace doesn't do anything special with the base pose, but
  // derived reference spaces (bounded, unbounded, stationary, etc.) have their
  // own custom behavior.
  TransformationMatrix identity;
  std::unique_ptr<TransformationMatrix> transform_matrix =
      TransformBasePose(identity);

  if (!transform_matrix) {
    // Transform wasn't possible.
    return nullptr;
  }

  // Must account for position and orientation defined by origin offset.
  transform_matrix->Multiply(origin_offset_->TransformMatrix());
  return transform_matrix;
}

TransformationMatrix XRReferenceSpace::OriginOffsetMatrix() {
  return origin_offset_->TransformMatrix();
}

TransformationMatrix XRReferenceSpace::InverseOriginOffsetMatrix() {
  return origin_offset_->InverseTransformMatrix();
}

XRReferenceSpace* XRReferenceSpace::getOffsetReferenceSpace(
    XRRigidTransform* additional_offset) {
  auto matrix =
      OriginOffsetMatrix().Multiply(additional_offset->TransformMatrix());

  auto* result_transform = MakeGarbageCollected<XRRigidTransform>(matrix);
  return cloneWithOriginOffset(result_transform);
}

XRReferenceSpace* XRReferenceSpace::cloneWithOriginOffset(
    XRRigidTransform* origin_offset) {
  return MakeGarbageCollected<XRReferenceSpace>(this->session(), origin_offset,
                                                type_);
}

void XRReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(origin_offset_);
  XRSpace::Trace(visitor);
}

void XRReferenceSpace::OnReset() {
  if (type_ != Type::kTypeViewer) {
    DispatchEvent(
        *XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
  }
}

}  // namespace blink
