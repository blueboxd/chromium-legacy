// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_world_information.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

const char kInactiveFrame[] =
    "XRFrame access outside the callback that produced it is invalid.";

const char kNonAnimationFrame[] =
    "getViewerPose can only be called on XRFrame objects passed to "
    "XRSession.requestAnimationFrame callbacks.";

const char kSessionMismatch[] = "XRSpace and XRFrame sessions do not match.";

const char kCannotReportPoses[] =
    "Poses cannot be given out for the current state.";

const char kHitTestSourceUnavailable[] =
    "Unable to obtain hit test results for specified hit test source. Ensure "
    "that it was not already canceled.";

}  // namespace

XRFrame::XRFrame(XRSession* session, XRWorldInformation* world_information)
    : world_information_(world_information), session_(session) {}

XRViewerPose* XRFrame::getViewerPose(XRReferenceSpace* reference_space,
                                     ExceptionState& exception_state) const {
  DVLOG(3) << __func__;
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!is_animation_frame_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNonAnimationFrame);
    return nullptr;
  }

  if (!reference_space) {
    DVLOG(1) << __func__ << ": reference space not present, returning null";
    return nullptr;
  }

  // Must use a reference space created from the same session.
  if (reference_space->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(kCannotReportPoses);
    return nullptr;
  }

  session_->LogGetPose();

  std::unique_ptr<TransformationMatrix> offset_space_from_viewer =
      reference_space->OffsetFromViewer();

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!(offset_space_from_viewer && offset_space_from_viewer->IsInvertible())) {
    DVLOG(1) << __func__
             << ": offset_space_from_viewer is invalid or not invertible - "
                "returning nullptr, offset_space_from_viewer valid? "
             << (offset_space_from_viewer ? true : false);
    return nullptr;
  }

  return MakeGarbageCollected<XRViewerPose>(session(),
                                            *offset_space_from_viewer);
}

XRAnchorSet* XRFrame::trackedAnchors() const {
  return session_->TrackedAnchors();
}

// Return an XRPose that has a transform of basespace_from_space, while
// accounting for the base pose matrix of this frame. If computing a transform
// isn't possible, return nullptr.
XRPose* XRFrame::getPose(XRSpace* space,
                         XRSpace* basespace,
                         ExceptionState& exception_state) {
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!space || !basespace) {
    DVLOG(2) << __func__ << " : space or basespace is null, space =" << space
             << ", basespace = " << basespace;
    return nullptr;
  }

  if (space->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (basespace->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(kCannotReportPoses);
    return nullptr;
  }

  return space->getPose(basespace);
}

void XRFrame::Deactivate() {
  is_active_ = false;
  is_animation_frame_ = false;
}

HeapVector<Member<XRHitTestResult>> XRFrame::getHitTestResults(
    XRHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return {};
  }

  return hit_test_source->Results();
}

HeapVector<Member<XRTransientInputHitTestResult>>
XRFrame::getHitTestResultsForTransientInput(
    XRTransientInputHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return {};
  }

  return hit_test_source->Results();
}

ScriptPromise XRFrame::createAnchor(ScriptState* script_state,
                                    XRRigidTransform* initial_pose,
                                    XRSpace* space,
                                    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return {};
  }

  if (!initial_pose) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kNoRigidTransformSpecified);
    return {};
  }

  if (!space) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kNoSpaceSpecified);
    return {};
  }

  auto maybe_mojo_from_offset_space = space->MojoFromOffsetMatrix();

  if (!maybe_mojo_from_offset_space) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    return ScriptPromise();
  }

  return session_->CreateAnchor(script_state, initial_pose->TransformMatrix(),
                                *maybe_mojo_from_offset_space, base::nullopt,
                                exception_state);
}

void XRFrame::Trace(Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(world_information_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
