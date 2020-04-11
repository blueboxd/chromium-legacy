// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"

namespace blink {

bool IgnorePaintTimingScope::should_ignore_ = false;

}  // namespace blink
