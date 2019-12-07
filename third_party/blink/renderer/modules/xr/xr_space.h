// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"
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

 protected:
  explicit XRSpace(XRSession* session);

 public:
  ~XRSpace() override;

  // Gets the pose of this space's native origin in mojo space. This transform
  // maps from this space to mojo space (aka device space). Unless noted
  // otherwise, all data returned over vr_service.mojom interfaces is expressed
  // in mojo space coordinates. Returns nullptr if computing a transform is not
  // possible.
  virtual std::unique_ptr<TransformationMatrix> MojoFromSpace() = 0;

  // Gets the pose of the mojo origin in this reference space, corresponding
  // to a transform from mojo coordinates to reference space coordinates.
  // Note that it is expected to be the inverse of the above.
  virtual std::unique_ptr<TransformationMatrix> SpaceFromMojo() = 0;

  // Gets the viewer pose in this space, corresponding to a transform from
  // viewer coordinates to this space's coordinates. (The position elements of
  // the transformation matrix are the viewer's location in this space's
  // coordinates.)
  // Prefer this helper method over querying SpaceFromMojo and multiplying
  // on the calling side, as this allows the viewer space to return identity
  // instead of something near to, but not quite, identity.
  virtual std::unique_ptr<TransformationMatrix> SpaceFromViewer(
      const TransformationMatrix* mojo_from_viewer);

  // Indicates whether or not the position portion of the native origin of this
  // space is emulated.
  virtual bool EmulatedPosition() const;

  // Gets the pose of this space's origin in |other_space|. This is a transform
  // that maps from this space to the other's space, or in other words:
  // other_from_this.
  virtual XRPose* getPose(XRSpace* other_space);

  // Gets the viewer pose in this space, including using an appropriate
  // default pose (i.e. if tracking is lost), and applying originOffset
  // as applicable. TODO(https://crbug.com/1008466): consider moving
  // the originOffset handling to a separate class?
  std::unique_ptr<TransformationMatrix> OffsetSpaceFromViewer();

  XRSession* session() const { return session_; }

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // Return origin offset matrix, aka native_origin_from_offset_space.
  virtual TransformationMatrix OriginOffsetMatrix();
  virtual TransformationMatrix InverseOriginOffsetMatrix();

  virtual base::Optional<XRNativeOriginInformation> NativeOrigin() const = 0;

  void Trace(blink::Visitor* visitor) override;

 protected:
  std::unique_ptr<TransformationMatrix> TryInvert(
      std::unique_ptr<TransformationMatrix> matrix);

 private:
  const Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
