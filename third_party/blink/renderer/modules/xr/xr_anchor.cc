// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRSpace* XRAnchor::anchorSpace() const {
  return nullptr;
}

TransformationMatrix XRAnchor::poseMatrix() const {
  return {};
}

double XRAnchor::lastChangedTime() const {
  return last_changed_time_;
}

void XRAnchor::detach() {}

void XRAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(anchor_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
