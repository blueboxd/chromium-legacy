// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRPlaneSet : public ScriptWrappable,
                   public SetlikeIterable<Member<XRPlane>> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRPlaneSet(HeapHashSet<Member<XRPlane>> planes);

  unsigned size() const;

  // Returns true if passed in |plane| is a member of the XRPlaneSet.
  bool hasForBinding(ScriptState* script_state,
                     XRPlane* plane,
                     ExceptionState& exception_state) const;

  void Trace(blink::Visitor* visitor) override;

 private:
  class IterationSource final
      : public SetlikeIterable<Member<XRPlane>>::IterationSource {
   public:
    explicit IterationSource(const HeapHashSet<Member<XRPlane>>& planes);

    bool Next(ScriptState* script_state,
              Member<XRPlane>& key,
              Member<XRPlane>& value,
              ExceptionState& exception_state) override;

    void Trace(blink::Visitor* visitor) override;

   private:
    HeapVector<Member<XRPlane>> planes_;

    unsigned index_;
  };

  // Starts iteration over XRPlaneSet.
  // Needed for SetlikeIterable to work properly.
  XRPlaneSet::IterationSource* StartIteration(
      ScriptState* script_state,
      ExceptionState& exception_state) override;

  HeapHashSet<Member<XRPlane>> planes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_
