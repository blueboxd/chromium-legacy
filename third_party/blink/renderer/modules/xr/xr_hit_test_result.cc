// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xr_hit_test_result.h"

namespace blink {

XRHitTestOptions* XRHitTestResult::hitTestOptions() const {
  return nullptr;
}

XRRigidTransform* XRHitTestResult::transform() const {
  return nullptr;
}

}  // namespace blink
