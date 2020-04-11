// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_IGNORE_PAINT_TIMING_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_IGNORE_PAINT_TIMING_SCOPE_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Creates a scope to ignore paint timing, e.g. when we are painting contents
// under opacity:0.
class PLATFORM_EXPORT IgnorePaintTimingScope {
  STACK_ALLOCATED();

 public:
  IgnorePaintTimingScope() : auto_reset_(&should_ignore_, true) {}
  ~IgnorePaintTimingScope() = default;

  static bool ShouldIgnore() { return should_ignore_; }

 private:
  base::AutoReset<bool> auto_reset_;
  static bool should_ignore_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_IGNORE_PAINT_TIMING_SCOPE_H_
