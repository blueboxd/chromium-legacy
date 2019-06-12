// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRPlane::XRPlane(XRSession* session,
                 const device::mojom::blink::XRPlaneDataPtr& plane_data,
                 double timestamp)
    : XRPlane(session,
              mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
                  plane_data->orientation),
              mojo::ConvertTo<blink::TransformationMatrix>(plane_data->pose),
              mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(
                  plane_data->polygon),
              timestamp) {}

XRPlane::XRPlane(XRSession* session,
                 const base::Optional<Orientation>& orientation,
                 const TransformationMatrix& pose_matrix,
                 const HeapVector<Member<DOMPointReadOnly>>& polygon,
                 double timestamp)
    : polygon_(polygon),
      orientation_(orientation),
      pose_matrix_(std::make_unique<TransformationMatrix>(pose_matrix)),
      session_(session),
      last_changed_time_(timestamp) {
  DVLOG(3) << __func__;
}

XRPose* XRPlane::getPose(XRReferenceSpace* reference_space) const {
  std::unique_ptr<TransformationMatrix> viewer_pose =
      reference_space->GetViewerPoseMatrix(pose_matrix_.get());
  return MakeGarbageCollected<XRPose>(*viewer_pose,
                                      session_->EmulatedPosition());
}

String XRPlane::orientation() const {
  if (orientation_.has_value()) {
    switch (orientation_.value()) {
      case Orientation::kHorizontal:
        return "Horizontal";
      case Orientation::kVertical:
        return "Vertical";
    }
  }
  return "";
}

double XRPlane::lastChangedTime() const {
  return last_changed_time_;
}

HeapVector<Member<DOMPointReadOnly>> XRPlane::polygon() const {
  // Returns copy of a vector - by design. This way, JavaScript code could
  // store the state of the plane's polygon in frame N just by storing the
  // array (`let polygon = plane.polygon`) - the stored array won't be affected
  // by the changes to the plane that could happen in frames >N.
  return polygon_;
}

void XRPlane::Update(const device::mojom::blink::XRPlaneDataPtr& plane_data,
                     double timestamp) {
  DVLOG(3) << __func__;

  last_changed_time_ = timestamp;

  orientation_ = mojo::ConvertTo<base::Optional<blink::XRPlane::Orientation>>(
      plane_data->orientation);
  pose_matrix_ = std::make_unique<TransformationMatrix>(
      mojo::ConvertTo<blink::TransformationMatrix>(plane_data->pose));
  polygon_ = mojo::ConvertTo<HeapVector<Member<DOMPointReadOnly>>>(
      plane_data->polygon);
}

void XRPlane::Trace(blink::Visitor* visitor) {
  visitor->Trace(polygon_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
