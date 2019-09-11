// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class TransformationMatrix;
class XRInputSource;
class XRPose;
class XRSession;

class XRSpace : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRSpace(XRSession*);
  ~XRSpace() override;

  // Get a transform that maps from this space to mojo space (aka device space).
  // Unless noted otherwise, all data returned over vr_service.mojom interfaces
  // is relative to mojo space.
  // Returns nullptr if computing a transform is not possible.
  virtual std::unique_ptr<TransformationMatrix> GetTransformToMojoSpace();

  virtual std::unique_ptr<TransformationMatrix> DefaultPose();
  virtual std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose);
  virtual std::unique_ptr<TransformationMatrix> TransformBaseInputPose(
      const TransformationMatrix& base_input_pose,
      const TransformationMatrix& base_pose);

  virtual XRPose* getPose(XRSpace* other_space,
                          const TransformationMatrix* base_pose_matrix);

  std::unique_ptr<TransformationMatrix> GetViewerPoseMatrix(
      const TransformationMatrix* base_pose_matrix);

  XRSession* session() const { return session_; }

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  virtual TransformationMatrix OriginOffsetMatrix();
  virtual TransformationMatrix InverseOriginOffsetMatrix();

  void Trace(blink::Visitor*) override;

 private:
  const Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
