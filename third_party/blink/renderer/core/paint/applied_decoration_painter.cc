// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

void AppliedDecorationPainter::Paint() {
  context_.SetStrokeStyle(decoration_info_.StrokeStyle());
  context_.SetStrokeColor(decoration_info_.LineColor());

  switch (decoration_info_.DecorationStyle()) {
    case ETextDecorationStyle::kWavy:
      StrokeWavyTextDecoration();
      break;
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      context_.SetShouldAntialias(decoration_info_.ShouldAntialias());
      FALLTHROUGH;
    default:
      context_.DrawLineForText(decoration_info_.StartPoint(line_),
                               decoration_info_.Width());

      if (decoration_info_.DecorationStyle() == ETextDecorationStyle::kDouble) {
        context_.DrawLineForText(
            decoration_info_.StartPoint(line_) +
                FloatPoint(0, decoration_info_.DoubleOffset(line_)),
            decoration_info_.Width());
      }
  }
}

void AppliedDecorationPainter::StrokeWavyTextDecoration() {
  context_.SetShouldAntialias(true);
  context_.StrokePath(decoration_info_.PrepareWavyStrokePath(line_).value());
}

}  // namespace blink
