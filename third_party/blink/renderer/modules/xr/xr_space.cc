// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session) : session_(session) {}

XRSpace::~XRSpace() = default;

std::unique_ptr<TransformationMatrix> XRSpace::SpaceFromViewer(
    const TransformationMatrix* mojo_from_viewer) {
  if (!mojo_from_viewer)
    return nullptr;

  std::unique_ptr<TransformationMatrix> space_from_mojo = SpaceFromMojo();
  if (!space_from_mojo)
    return nullptr;

  space_from_mojo->Multiply(*mojo_from_viewer);

  // This is now space_from_viewer
  return space_from_mojo;
  ;
}

TransformationMatrix XRSpace::OriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

TransformationMatrix XRSpace::InverseOriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

std::unique_ptr<TransformationMatrix> XRSpace::TryInvert(
    std::unique_ptr<TransformationMatrix> matrix) {
  if (!matrix)
    return nullptr;

  DCHECK(matrix->IsInvertible());
  return std::make_unique<TransformationMatrix>(matrix->Inverse());
}

bool XRSpace::EmulatedPosition() const {
  return session()->EmulatedPosition();
}

XRPose* XRSpace::getPose(XRSpace* other_space) {
  std::unique_ptr<TransformationMatrix> mojo_from_space = MojoFromSpace();
  if (!mojo_from_space) {
    return nullptr;
  }

  // Add any origin offset now.
  mojo_from_space->Multiply(OriginOffsetMatrix());

  std::unique_ptr<TransformationMatrix> other_from_mojo =
      other_space->SpaceFromMojo();
  if (!other_from_mojo)
    return nullptr;

  TransformationMatrix offset_other_from_mojo =
      other_space->InverseOriginOffsetMatrix().Multiply(*other_from_mojo);

  // TODO(crbug.com/969133): Update how EmulatedPosition is determined here once
  // spec issue https://github.com/immersive-web/webxr/issues/534 has been
  // resolved.
  TransformationMatrix other_from_space =
      offset_other_from_mojo.Multiply(*mojo_from_space);
  return MakeGarbageCollected<XRPose>(
      other_from_space, EmulatedPosition() || other_space->EmulatedPosition());
}

std::unique_ptr<TransformationMatrix> XRSpace::OffsetSpaceFromViewer() {
  std::unique_ptr<TransformationMatrix> space_from_viewer =
      SpaceFromViewer(base::OptionalOrNullptr(session()->MojoFromViewer()));

  if (!space_from_viewer) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so that
  // things like teleportation works.
  //
  // This is offset_from_viewer = offset_from_space * space_from_viewer,
  // where offset_from_viewer = inverse(viewer_from_offset).
  // TODO(https://crbug.com/1008466): move originOffset to separate class?
  return std::make_unique<TransformationMatrix>(
      InverseOriginOffsetMatrix().Multiply(*space_from_viewer));
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

base::Optional<XRNativeOriginInformation> XRSpace::NativeOrigin() const {
  return base::nullopt;
}

void XRSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
