// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRHitTestOptions;
class XRRigidTransform;

class XRHitTestResult : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRHitTestOptions* hitTestOptions() const;
  XRRigidTransform* transform() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_
