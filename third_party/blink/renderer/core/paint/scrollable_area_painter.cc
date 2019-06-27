// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollbar_painter.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

void ScrollableAreaPainter::PaintResizer(GraphicsContext& context,
                                         const IntPoint& paint_offset,
                                         const CullRect& cull_rect) {
  if (GetScrollableArea().GetLayoutBox()->StyleRef().Resize() == EResize::kNone)
    return;

  IntRect abs_rect = GetScrollableArea().ResizerCornerRect(
      GetScrollableArea().GetLayoutBox()->PixelSnappedBorderBoxRect(
          GetScrollableArea().Layer()->SubpixelAccumulation()),
      kResizerForPointer);
  if (abs_rect.IsEmpty())
    return;
  abs_rect.MoveBy(paint_offset);

  if (const auto* resizer = GetScrollableArea().Resizer()) {
    if (!cull_rect.Intersects(abs_rect))
      return;
    ScrollbarPainter::PaintIntoRect(*resizer, context,
                                    PhysicalOffset(paint_offset),
                                    PhysicalRect(abs_rect));
    return;
  }

  const auto& client = DisplayItemClientForCorner();
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                  DisplayItem::kResizer))
    return;

  DrawingRecorder recorder(context, client, DisplayItem::kResizer);

  DrawPlatformResizerImage(context, abs_rect);

  // Draw a frame around the resizer (1px grey line) if there are any scrollbars
  // present.  Clipping will exclude the right and bottom edges of this frame.
  if (!GetScrollableArea().HasOverlayScrollbars() &&
      GetScrollableArea().HasScrollbar()) {
    GraphicsContextStateSaver state_saver(context);
    context.Clip(abs_rect);
    IntRect larger_corner = abs_rect;
    larger_corner.SetSize(
        IntSize(larger_corner.Width() + 1, larger_corner.Height() + 1));
    context.SetStrokeColor(Color(217, 217, 217));
    context.SetStrokeThickness(1.0f);
    context.SetFillColor(Color::kTransparent);
    context.DrawRect(larger_corner);
  }
}

void ScrollableAreaPainter::DrawPlatformResizerImage(
    GraphicsContext& context,
    IntRect resizer_corner_rect) {
  IntPoint points[4];
  bool on_left = false;
  if (GetScrollableArea()
          .GetLayoutBox()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    on_left = true;
    points[0].SetX(resizer_corner_rect.X() + 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() * 3 / 4);
  } else {
    points[0].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() - 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() +
                   resizer_corner_rect.Width() * 3 / 4);
  }
  points[0].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() / 2);
  points[1].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() - 1);
  points[2].SetY(resizer_corner_rect.Y() +
                 resizer_corner_rect.Height() * 3 / 4);
  points[3].SetY(points[1].Y());

  PaintFlags paint_flags;
  paint_flags.setStyle(PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(1);

  SkPath line_path;

  // Draw a dark line, to ensure contrast against a light background
  line_path.moveTo(points[0].X(), points[0].Y());
  line_path.lineTo(points[1].X(), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y());
  line_path.lineTo(points[3].X(), points[3].Y());
  paint_flags.setColor(SkColorSetARGB(153, 0, 0, 0));
  context.DrawPath(line_path, paint_flags);

  // Draw a light line one pixel below the light line,
  // to ensure contrast against a dark background
  line_path.rewind();
  line_path.moveTo(points[0].X(), points[0].Y() + 1);
  line_path.lineTo(points[1].X() + (on_left ? -1 : 1), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y() + 1);
  line_path.lineTo(points[3].X() + (on_left ? -1 : 1), points[3].Y());
  paint_flags.setColor(SkColorSetARGB(153, 255, 255, 255));
  context.DrawPath(line_path, paint_flags);
}

void ScrollableAreaPainter::PaintOverflowControls(
    const PaintInfo& paint_info,
    const IntPoint& paint_offset) {
  // Don't do anything if we have no overflow.
  const auto& box = *GetScrollableArea().GetLayoutBox();
  if (!box.HasOverflowClip() ||
      box.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  // Overlay scrollbars are painted in the dedicated paint phase, and normal
  // scrollbars are painted in the background paint phase.
  if (GetScrollableArea().HasOverlayScrollbars()) {
    if (paint_info.phase != PaintPhase::kOverlayScrollbars)
      return;
  } else if (!ShouldPaintSelfBlockBackground(paint_info.phase)) {
    return;
  }

  GraphicsContext& context = paint_info.context;
  const auto* fragment = paint_info.FragmentToPaint(box);
  if (!fragment)
    return;

  base::Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
  const auto* properties = fragment->PaintProperties();
  // TODO(crbug.com/849278): Remove either the DCHECK or the if condition
  // when we figure out in what cases that the box doesn't have properties.
  DCHECK(properties);
  if (properties) {
    if (const auto* clip = properties->OverflowControlsClip()) {
      scoped_paint_chunk_properties.emplace(context.GetPaintController(), *clip,
                                            box,
                                            DisplayItem::kOverflowControls);
    }
  }

  CullRect adjusted_cull_rect = paint_info.GetCullRect();
  adjusted_cull_rect.MoveBy(-paint_offset);

  if (GetScrollableArea().HorizontalScrollbar() &&
      !GetScrollableArea().LayerForHorizontalScrollbar()) {
    GetScrollableArea().HorizontalScrollbar()->Paint(context,
                                                     adjusted_cull_rect);
  }
  if (GetScrollableArea().VerticalScrollbar() &&
      !GetScrollableArea().LayerForVerticalScrollbar()) {
    GetScrollableArea().VerticalScrollbar()->Paint(context, adjusted_cull_rect);
  }

  if (!GetScrollableArea().LayerForScrollCorner()) {
    // We fill our scroll corner with white if we have a scrollbar that doesn't
    // run all the way up to the edge of the box.
    PaintScrollCorner(context, paint_offset, paint_info.GetCullRect());

    // Paint our resizer last, since it sits on top of the scroll corner.
    PaintResizer(context, paint_offset, paint_info.GetCullRect());
  }
}

void ScrollableAreaPainter::PaintScrollCorner(GraphicsContext& context,
                                              const IntPoint& paint_offset,
                                              const CullRect& cull_rect) {
  IntRect abs_rect = GetScrollableArea().ScrollCornerRect();
  if (abs_rect.IsEmpty())
    return;
  abs_rect.MoveBy(paint_offset);

  if (const auto* scroll_corner = GetScrollableArea().ScrollCorner()) {
    if (!cull_rect.Intersects(abs_rect))
      return;
    ScrollbarPainter::PaintIntoRect(*scroll_corner, context,
                                    PhysicalOffset(paint_offset),
                                    PhysicalRect(abs_rect));
    return;
  }

  // We don't want to paint opaque if we have overlay scrollbars, since we need
  // to see what is behind it.
  if (GetScrollableArea().HasOverlayScrollbars())
    return;

  ScrollbarTheme* theme = nullptr;

  if (GetScrollableArea().HorizontalScrollbar()) {
    theme = &GetScrollableArea().HorizontalScrollbar()->GetTheme();
  } else if (GetScrollableArea().VerticalScrollbar()) {
    theme = &GetScrollableArea().VerticalScrollbar()->GetTheme();
  } else {
    NOTREACHED();
  }

  const auto& client = DisplayItemClientForCorner();
  theme->PaintScrollCorner(context, client, abs_rect);
}

PaintLayerScrollableArea& ScrollableAreaPainter::GetScrollableArea() const {
  return *scrollable_area_;
}

const DisplayItemClient& ScrollableAreaPainter::DisplayItemClientForCorner()
    const {
  if (const auto* graphics_layer = GetScrollableArea().LayerForScrollCorner())
    return *graphics_layer;
  return *GetScrollableArea().GetLayoutBox();
}

}  // namespace blink
