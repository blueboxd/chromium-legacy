// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_

#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;

// Helper class that returns an XRSpace that tracks the position of object of
// type T (for example XRPlane, XRAnchor). The type T has to have a poseMatrix()
// method.
template <typename T>
class XRObjectSpace : public XRSpace {
 public:
  explicit XRObjectSpace(XRSession* session, const T* object)
      : XRSpace(session), object_(object) {}

  std::unique_ptr<TransformationMatrix> MojoFromNative() override {
    auto maybe_mojo_from_object = object_->MojoFromObject();
    if (maybe_mojo_from_object) {
      return std::make_unique<TransformationMatrix>(*maybe_mojo_from_object);
    } else {
      return nullptr;
    }
  }

  std::unique_ptr<TransformationMatrix> NativeFromMojo() final {
    return TryInvert(MojoFromNative());
  }

  base::Optional<XRNativeOriginInformation> NativeOrigin() const override {
    return XRNativeOriginInformation::Create(object_);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(object_);
    XRSpace::Trace(visitor);
  }

 private:
  Member<const T> object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_
