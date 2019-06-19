// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane_set.h"

namespace blink {

XRPlaneSet::XRPlaneSet(HeapHashSet<Member<XRPlane>> planes) : planes_(planes) {}

unsigned XRPlaneSet::size() const {
  return planes_.size();
}

bool XRPlaneSet::hasForBinding(ScriptState*,
                               XRPlane* plane,
                               ExceptionState&) const {
  DCHECK(plane);

  return planes_.find(plane) != planes_.end();
}

XRPlaneSet::IterationSource* XRPlaneSet::StartIteration(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<XRPlaneSet::IterationSource>(planes_);
}

void XRPlaneSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(planes_);
  ScriptWrappable::Trace(visitor);
}

XRPlaneSet::IterationSource::IterationSource(
    const HeapHashSet<Member<XRPlane>>& planes)
    : index_(0) {
  for (auto plane : planes) {
    planes_.push_back(plane);
  }
}

bool XRPlaneSet::IterationSource::Next(ScriptState* script_state,
                                       Member<XRPlane>& key,
                                       Member<XRPlane>& value,
                                       ExceptionState& exception_state) {
  if (index_ >= planes_.size()) {
    return false;
  }

  key = value = planes_[index_];
  ++index_;

  return true;
}

void XRPlaneSet::IterationSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(planes_);
  SetlikeIterable<Member<XRPlane>>::IterationSource::Trace(visitor);
}

}  // namespace blink
