// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_mathml_painter.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void NGMathMLPainter::PaintBar(const PaintInfo& info, const IntRect& bar) {
  if (bar.IsEmpty())
    return;

  GraphicsContextStateSaver state_saver(info.context);
  info.context.SetStrokeThickness(bar.Height());
  info.context.SetStrokeStyle(kSolidStroke);
  info.context.SetStrokeColor(
      box_fragment_.Style().VisitedDependentColor(GetCSSPropertyColor()));
  IntPoint line_end_point = {bar.Width(), 0};
  info.context.DrawLine(bar.Location(), bar.Location() + line_end_point);
}

void NGMathMLPainter::PaintFractionBar(const PaintInfo& info,
                                       PhysicalOffset paint_offset) {
  const DisplayItemClient& display_item_client =
      *box_fragment_.GetLayoutObject();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          info.context, display_item_client, info.phase))
    return;

  DrawingRecorder recorder(info.context, display_item_client, info.phase);

  DCHECK(box_fragment_.Style().IsHorizontalWritingMode());
  const ComputedStyle& style = box_fragment_.Style();
  LayoutUnit line_thickness = FractionLineThickness(style);
  if (!line_thickness)
    return;
  LayoutUnit axis_height = MathAxisHeight(style);
  if (auto baseline = box_fragment_.Baseline()) {
    auto borders = box_fragment_.Borders();
    auto padding = box_fragment_.Padding();
    PhysicalRect bar_rect = {
        borders.left + padding.left, *baseline - axis_height,
        box_fragment_.Size().width - borders.HorizontalSum() -
            padding.HorizontalSum(),
        line_thickness};
    bar_rect.Move(paint_offset);
    PaintBar(info, PixelSnappedIntRect(bar_rect));
  }
}

}  // namespace blink
