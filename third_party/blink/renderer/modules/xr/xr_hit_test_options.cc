// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_options.h"

#include "third_party/blink/renderer/modules/xr/xr_hit_test_options_init.h"
#include "third_party/blink/renderer/modules/xr/xr_ray.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRHitTestOptions::XRHitTestOptions(XRHitTestOptionsInit* options_init) {
  DCHECK(options_init);
  DCHECK(options_init->hasSpace());  // Is it enforced by generated bindings?

  space_ = options_init->space();

  if (options_init->hasOffsetRay()) {
    ray_ = options_init->offsetRay();
  } else {
    ray_ = MakeGarbageCollected<XRRay>();
  }
}

void XRHitTestOptions::Trace(blink::Visitor* visitor) {
  visitor->Trace(space_);
  visitor->Trace(ray_);
  ScriptWrappable::Trace(visitor);
}

XRSpace* XRHitTestOptions::space() const {
  return space_;
}

XRRay* XRHitTestOptions::offsetRay() const {
  return ray_;
}

}  // namespace blink
