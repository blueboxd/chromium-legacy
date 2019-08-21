// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_

#include <memory>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;
class XRSpace;

class XRAnchor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRSpace* anchorSpace() const;

  TransformationMatrix poseMatrix() const;

  double lastChangedTime() const;

  void detach();

  void Trace(blink::Visitor* visitor) override;

 private:
  // Anchor's pose in device (mojo) space.
  std::unique_ptr<TransformationMatrix> pose_matrix_;

  Member<XRSession> session_;

  double last_changed_time_;

  // Cached anchor space - it will be created by `anchorSpace()` if it's not
  // set.
  mutable Member<XRSpace> anchor_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
