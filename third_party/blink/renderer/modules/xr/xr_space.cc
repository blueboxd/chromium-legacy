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

std::unique_ptr<TransformationMatrix> XRSpace::NativeFromViewer(
    const TransformationMatrix* mojo_from_viewer) {
  if (!mojo_from_viewer)
    return nullptr;

  std::unique_ptr<TransformationMatrix> native_from_mojo = NativeFromMojo();
  if (!native_from_mojo)
    return nullptr;

  native_from_mojo->Multiply(*mojo_from_viewer);

  // This is now native_from_viewer
  return native_from_mojo;
  ;
}

TransformationMatrix XRSpace::NativeFromOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

TransformationMatrix XRSpace::OffsetFromNativeMatrix() {
  TransformationMatrix identity;
  return identity;
}

std::unique_ptr<TransformationMatrix> XRSpace::MojoFromOffsetMatrix() {
  auto maybe_mojo_from_native = MojoFromNative();
  if (!maybe_mojo_from_native) {
    return nullptr;
  }

  // Modifies maybe_mojo_from_native - it becomes mojo_from_offset_matrix.
  // Saves a heap allocation since there is no need to create a new unique_ptr.
  maybe_mojo_from_native->Multiply(NativeFromOffsetMatrix());
  return maybe_mojo_from_native;
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
  // Named mojo_from_offset because that is what we will leave it as, though it
  // starts mojo_from_native.
  std::unique_ptr<TransformationMatrix> mojo_from_offset = MojoFromNative();
  if (!mojo_from_offset) {
    return nullptr;
  }

  // Add any origin offset now.
  mojo_from_offset->Multiply(NativeFromOffsetMatrix());

  std::unique_ptr<TransformationMatrix> other_from_mojo =
      other_space->NativeFromMojo();
  if (!other_from_mojo)
    return nullptr;

  // Add any origin offset from the other space now.
  TransformationMatrix other_offset_from_mojo =
      other_space->OffsetFromNativeMatrix().Multiply(*other_from_mojo);

  // TODO(crbug.com/969133): Update how EmulatedPosition is determined here once
  // spec issue https://github.com/immersive-web/webxr/issues/534 has been
  // resolved.
  TransformationMatrix other_offset_from_offset =
      other_offset_from_mojo.Multiply(*mojo_from_offset);
  return MakeGarbageCollected<XRPose>(
      other_offset_from_offset,
      EmulatedPosition() || other_space->EmulatedPosition());
}

std::unique_ptr<TransformationMatrix> XRSpace::OffsetFromViewer() {
  std::unique_ptr<TransformationMatrix> native_from_viewer =
      NativeFromViewer(base::OptionalOrNullptr(session()->MojoFromViewer()));

  if (!native_from_viewer) {
    return nullptr;
  }

  return std::make_unique<TransformationMatrix>(
      OffsetFromNativeMatrix().Multiply(*native_from_viewer));
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

void XRSpace::Trace(Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
