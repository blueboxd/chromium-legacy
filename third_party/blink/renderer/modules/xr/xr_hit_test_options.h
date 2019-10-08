// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_OPTIONS_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRRay;
class XRSpace;
class XRHitTestOptionsInit;

class XRHitTestOptions : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRHitTestOptions(XRHitTestOptionsInit* options_init);

  XRSpace* space() const;
  XRRay* offsetRay() const;

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<XRSpace> space_;
  Member<XRRay> ray_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_OPTIONS_H_
