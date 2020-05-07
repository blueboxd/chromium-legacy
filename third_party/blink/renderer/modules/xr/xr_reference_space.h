// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRRigidTransform;

class XRReferenceSpace : public XRSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Used for metrics, don't remove or change values.
  enum class Type : int {
    kTypeViewer = 0,
    kTypeLocal = 1,
    kTypeLocalFloor = 2,
    kTypeBoundedFloor = 3,
    kTypeUnbounded = 4,
    kMaxValue = kTypeUnbounded,
  };

  static Type StringToReferenceSpaceType(const String& reference_space_type);

  XRReferenceSpace(XRSession* session, Type type);
  XRReferenceSpace(XRSession* session,
                   XRRigidTransform* origin_offset,
                   Type type);
  ~XRReferenceSpace() override;

  base::Optional<TransformationMatrix> NativeFromMojo() override;
  base::Optional<TransformationMatrix> NativeFromViewer(
      const base::Optional<TransformationMatrix>& mojo_from_viewer) override;

  // MojoFromNative is final to enforce that children should be returning
  // NativeFromMojo, since this is simply written to always provide the inverse
  // of NativeFromMojo
  base::Optional<TransformationMatrix> MojoFromNative() final;

  TransformationMatrix NativeFromOffsetMatrix() override;
  TransformationMatrix OffsetFromNativeMatrix() override;

  // We override getPose to ensure that the viewer pose in viewer space returns
  // the identity pose instead of the result of multiplying inverse matrices.
  XRPose* getPose(XRSpace* other_space) override;

  Type GetType() const;

  XRReferenceSpace* getOffsetReferenceSpace(XRRigidTransform* transform);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reset, kReset)

  base::Optional<XRNativeOriginInformation> NativeOrigin() const override;

  void Trace(Visitor*) override;

  virtual void OnReset();

 private:
  virtual XRReferenceSpace* cloneWithOriginOffset(
      XRRigidTransform* origin_offset);

  void SetFloorFromMojo();

  unsigned int display_info_id_ = 0;

  std::unique_ptr<TransformationMatrix> floor_from_mojo_;
  Member<XRRigidTransform> origin_offset_;
  Type type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_
