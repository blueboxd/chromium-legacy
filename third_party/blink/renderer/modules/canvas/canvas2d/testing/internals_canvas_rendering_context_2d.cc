// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/testing/internals_canvas_rendering_context_2d.h"

#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

namespace blink {

uint32_t InternalsCanvasRenderingContext2D::countHitRegions(
    Internals&,
    CanvasRenderingContext2D* context) {
  return context->HitRegionsCount();
}

}  // namespace blink
