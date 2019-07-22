// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fieldset_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/hit_test_display_item.h"

namespace blink {

namespace {

LayoutRectOutsets BoxStrutToLayoutRectOutsets(
    const NGPixelSnappedPhysicalBoxStrut& box_strut) {
  return LayoutRectOutsets(
      LayoutUnit(box_strut.top), LayoutUnit(box_strut.right),
      LayoutUnit(box_strut.bottom), LayoutUnit(box_strut.left));
}

inline bool IsVisibleToPaint(const NGPhysicalFragment& fragment,
                             const ComputedStyle& style) {
  return !fragment.IsHiddenForPaint() &&
         style.Visibility() == EVisibility::kVisible;
}

bool FragmentVisibleToHitTestRequest(const NGPaintFragment& paint_fragment,
                                     const HitTestRequest& request) {
  const NGPhysicalFragment& fragment = paint_fragment.PhysicalFragment();
  const ComputedStyle& style = fragment.Style();
  return IsVisibleToPaint(fragment, style) &&
         (request.IgnorePointerEventsNone() ||
          style.PointerEvents() != EPointerEvents::kNone);
}

// Hit tests inline ancestor elements of |fragment| who do not have their own
// box fragments.
// @param physical_offset Physical offset of |fragment| in the paint layer.
bool HitTestCulledInlineAncestors(HitTestResult& result,
                                  const NGPaintFragment& fragment,
                                  const NGPaintFragment* previous_sibling,
                                  const HitTestLocation& hit_test_location,
                                  const PhysicalOffset& physical_offset) {
  DCHECK(fragment.Parent());
  DCHECK(fragment.PhysicalFragment().IsInline());
  const NGPaintFragment& parent = *fragment.Parent();
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const PhysicalOffset fallback_accumulated_offset =
      physical_offset - fragment.InlineOffsetToContainerBox();
  const LayoutObject* limit_layout_object =
      parent.PhysicalFragment().IsLineBox() ? parent.Parent()->GetLayoutObject()
                                            : parent.GetLayoutObject();

  const LayoutObject* current_layout_object = fragment.GetLayoutObject();
  for (LayoutObject* culled_parent = current_layout_object->Parent();
       culled_parent && culled_parent != limit_layout_object;
       culled_parent = culled_parent->Parent()) {
    // |culled_parent| is a culled inline element to be hit tested, since it's
    // "between" |fragment| and |fragment->Parent()| but doesn't have its own
    // box fragment.
    // To ensure the correct hit test ordering, |culled_parent| must be hit
    // tested only once after all of its descendants are hit tested:
    // - Shortcut: when |current_layout_object| is the only child (of
    // |culled_parent|), since it's just hit tested, we can safely hit test its
    // parent;
    // - General case: we hit test |culled_parent| only when it is not an
    // ancestor of |previous_sibling|; otherwise, |previous_sibling| has to be
    // hit tested first.
    // TODO(crbug.com/849331): It's wrong for bidi inline fragmentation. Fix it.
    const bool has_sibling = current_layout_object->PreviousSibling() ||
                             current_layout_object->NextSibling();
    if (has_sibling && previous_sibling &&
        previous_sibling->GetLayoutObject()->IsDescendantOf(culled_parent))
      break;

    if (culled_parent->IsLayoutInline() &&
        ToLayoutInline(culled_parent)
            ->HitTestCulledInline(result, hit_test_location,
                                  fallback_accumulated_offset, &parent))
      return true;

    current_layout_object = culled_parent;
  }

  return false;
}

// Returns if this fragment may not be laid out by LayoutNG.
bool FragmentRequiresLegacyFallback(const NGPhysicalFragment& fragment) {
  // Fallback to LayoutObject if this is a root of NG block layout.
  // If this box is for this painter, LayoutNGBlockFlow will call this back.
  // Otherwise it calls legacy painters.
  return fragment.IsBlockFormattingContextRoot();
}

}  // anonymous namespace

const NGPhysicalBoxFragment& NGBoxFragmentPainter::PhysicalFragment() const {
  return static_cast<const NGPhysicalBoxFragment&>(
      box_fragment_.PhysicalFragment());
}

const NGBorderEdges& NGBoxFragmentPainter::BorderEdges() const {
  if (border_edges_.has_value())
    return *border_edges_;
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  border_edges_ = NGBorderEdges::FromPhysical(
      fragment.BorderEdges(), fragment.Style().GetWritingMode());
  return *border_edges_;
}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  if (PhysicalFragment().IsAtomicInline() &&
      !box_fragment_.HasSelfPaintingLayer())
    PaintAtomicInline(paint_info);
  else
    PaintInternal(paint_info);
}

void NGBoxFragmentPainter::PaintInternal(const PaintInfo& paint_info) {
  ScopedPaintState paint_state(box_fragment_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  PaintInfo& info = paint_state.MutablePaintInfo();
  PhysicalOffset paint_offset = paint_state.PaintOffset();
  PaintPhase original_phase = info.phase;

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    PaintObject(info, paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly) {
    if ((original_phase == PaintPhase::kForeground ||
         original_phase == PaintPhase::kFloat ||
         original_phase == PaintPhase::kDescendantOutlinesOnly) &&
        box_fragment_.GetLayoutObject()->IsBox()) {
      ScopedBoxContentsPaintState contents_paint_state(
          paint_state, ToLayoutBox(*box_fragment_.GetLayoutObject()));
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    } else {
      PaintObject(info, paint_offset);
    }
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  // We paint scrollbars after we painted other things, so that the scrollbars
  // will sit above them.
  info.phase = original_phase;
  if (box_fragment_.HasOverflowClip()) {
    ScrollableAreaPainter(*PhysicalFragment().Layer()->GetScrollableArea())
        .PaintOverflowControls(info, RoundedIntPoint(paint_offset));
  }
}

void NGBoxFragmentPainter::RecordHitTestData(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const NGPhysicalFragment& physical_fragment = PhysicalFragment();
  // TODO(pdr): If we are painting the background into the scrolling contents
  // layer, we need to use the overflow rect instead of the border box rect. We
  // may want to move the call to RecordHitTestRect into
  // BoxPainter::PaintBoxDecorationBackgroundWithRect and share the logic
  // the background painting code already uses.
  PhysicalRect border_box = physical_fragment.LocalRect();
  if (physical_fragment.IsInline())
    border_box.offset += box_fragment_.InlineOffsetToContainerBox();
  border_box.offset += paint_offset;
  HitTestDisplayItem::Record(
      paint_info.context, box_fragment_,
      HitTestRect(border_box.ToLayoutRect(),
                  physical_fragment.EffectiveAllowedTouchAction()));
}

void NGBoxFragmentPainter::RecordHitTestDataForLine(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const NGPaintFragment& line) {
  PhysicalRect border_box = line.PhysicalFragment().LocalRect();
  border_box.offset += paint_offset;
  HitTestDisplayItem::Record(
      paint_info.context, line,
      HitTestRect(border_box.ToLayoutRect(),
                  PhysicalFragment().EffectiveAllowedTouchAction()));
}

void NGBoxFragmentPainter::PaintObject(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  const PaintPhase paint_phase = paint_info.phase;
  const NGPhysicalBoxFragment& physical_box_fragment = PhysicalFragment();
  const ComputedStyle& style = box_fragment_.Style();
  bool is_visible = IsVisibleToPaint(physical_box_fragment, style);

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    if (!suppress_box_decoration_background && is_visible)
      PaintBoxDecorationBackground(paint_info, paint_offset);

    if (NGFragmentPainter::ShouldRecordHitTestData(paint_info,
                                                   physical_box_fragment))
      RecordHitTestData(paint_info, paint_offset);

    // Record the scroll hit test after the background so background squashing
    // is not affected. Hit test order would be equivalent if this were
    // immediately before the background.
    // if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    //  PaintScrollHitTestDisplayItem(paint_info);

    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == PaintPhase::kMask && is_visible)
    return PaintMask(paint_info, paint_offset);

  if (paint_phase == PaintPhase::kForeground && paint_info.IsPrinting()) {
    NGFragmentPainter(box_fragment_)
        .AddPDFURLRectIfNeeded(paint_info, paint_offset);
  }

  if (paint_phase != PaintPhase::kSelfOutlineOnly) {
    if (physical_box_fragment.ChildrenInline()) {
      if (paint_phase != PaintPhase::kFloat) {
        if (physical_box_fragment.IsBlockFlow()) {
          PaintBlockFlowContents(paint_info, paint_offset);
        } else if (ShouldPaintDescendantOutlines(paint_info.phase)) {
          // TODO(kojii): |PaintInlineChildrenOutlines()| should do the work
          // instead. Legacy does so, and is more efficient. But NG outline
          // logic currently depends on |PaintInlineChildren()|.
          PaintInlineChildren(box_fragment_.Children(),
                              paint_info.ForDescendants(), paint_offset);
        } else {
          PaintInlineChildren(box_fragment_.Children(), paint_info,
                              paint_offset);
        }
      }

      if (paint_phase == PaintPhase::kFloat ||
          paint_phase == PaintPhase::kSelection ||
          paint_phase == PaintPhase::kTextClip) {
        if (physical_box_fragment.HasFloatingDescendants())
          PaintFloats(paint_info);
      }
    } else {
      PaintBlockChildren(paint_info);
    }
  }

  if (ShouldPaintSelfOutline(paint_phase))
    NGFragmentPainter(box_fragment_).PaintOutline(paint_info, paint_offset);

  // If the caret's node's fragment's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (paint_phase == PaintPhase::kForeground &&
      physical_box_fragment.ShouldPaintCarets())
    PaintCarets(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintCarets(const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  LocalFrame* frame = fragment.GetLayoutObject()->GetFrame();
  if (fragment.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (fragment.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

void NGBoxFragmentPainter::PaintBlockFlowContents(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  const LayoutObject* layout_object = fragment.GetLayoutObject();

  DCHECK(fragment.ChildrenInline());

  // Check if there were contents to be painted and return early if none.
  // The union of |ContentsInkOverflow()| and |LocalRect()| covers the rect to
  // check, in both cases of:
  // 1. Painting non-scrolling contents.
  // 2. Painting scrolling contents.
  // For 1, check with |ContentsInkOverflow()|, except when there is no
  // overflow, in which case check with |LocalRect()|. For 2, check with
  // |LayoutOverflow()|, but this can be approximiated with
  // |ContentsInkOverflow()|.
  PhysicalRect content_ink_rect = fragment.LocalRect();
  content_ink_rect.Unite(box_fragment_.ContentsInkOverflow());
  content_ink_rect.offset += PhysicalOffset(paint_offset);
  if (!paint_info.GetCullRect().Intersects(content_ink_rect.ToLayoutRect()))
    return;

  if (paint_info.phase == PaintPhase::kMask) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  DCHECK(layout_object->IsLayoutBlockFlow());
  const auto& layout_block = To<LayoutBlock>(*layout_object);
  DCHECK(layout_block.ChildrenInline());
  PaintLineBoxChildren(box_fragment_.Children(), paint_info.ForDescendants(),
                       paint_offset);
}

void NGBoxFragmentPainter::PaintBlockChildren(const PaintInfo& paint_info) {
  for (const NGPaintFragment* child : box_fragment_.Children()) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (child->HasSelfPaintingLayer() || fragment.IsFloating())
      continue;

    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (FragmentRequiresLegacyFallback(fragment))
        fragment.GetLayoutObject()->Paint(paint_info);
      else
        NGBoxFragmentPainter(*child).Paint(paint_info);
    } else {
      DCHECK(fragment.Type() == NGPhysicalFragment::kFragmentRenderedLegend)
          << fragment;
    }
  }
}

void NGBoxFragmentPainter::PaintFloatingChildren(
    NGPaintFragment::ChildList children,
    const PaintInfo& paint_info) {
  for (const NGPaintFragment* child : children) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (child->HasSelfPaintingLayer())
      continue;
    if (fragment.IsFloating()) {
      // TODO(kojii): The float is outside of the inline formatting context and
      // that it maybe another NG inline formatting context, NG block layout, or
      // legacy. NGBoxFragmentPainter can handle only the first case. In order
      // to cover more tests for other two cases, we always fallback to legacy,
      // which will forward back to NGBoxFragmentPainter if the float is for
      // NGBoxFragmentPainter. We can shortcut this for the first case when
      // we're more stable.
      ObjectPainter(*child->GetLayoutObject())
          .PaintAllPhasesAtomically(paint_info);
      continue;
    }
    if (const auto* child_container =
            DynamicTo<NGPhysicalContainerFragment>(fragment)) {
      if (child_container->HasFloatingDescendants())
        PaintFloatingChildren(child->Children(), paint_info);
    }
  }
}

void NGBoxFragmentPainter::PaintFloats(const PaintInfo& paint_info) {
  DCHECK(PhysicalFragment().HasFloatingDescendants());

  // TODO(eae): The legacy paint code currently handles most floats, if they can
  // be painted by PaintNG BlockFlowPainter::PaintFloats will then call
  // NGBlockFlowPainter::Paint on each float.
  // This code is currently only used for floats within a block within inline
  // children.
  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;
  PaintFloatingChildren(box_fragment_.Children(), float_paint_info);
}

void NGBoxFragmentPainter::PaintMask(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  const NGPhysicalBoxFragment& physical_box_fragment = PhysicalFragment();
  const ComputedStyle& style = physical_box_fragment.Style();
  if (!style.HasMask() || !IsVisibleToPaint(physical_box_fragment, style))
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, box_fragment_, paint_info.phase))
    return;

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));

  DrawingRecorder recorder(paint_info.context, box_fragment_, paint_info.phase);
  PhysicalRect paint_rect(paint_offset, box_fragment_.Size());
  const NGBorderEdges& border_edges = BorderEdges();
  PaintMaskImages(paint_info, paint_rect, *box_fragment_.GetLayoutObject(),
                  geometry, border_edges.line_left, border_edges.line_right);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  if (box_fragment_.PhysicalFragment().IsFieldsetContainer()) {
    NGFieldsetPainter(box_fragment_)
        .PaintBoxDecorationBackground(paint_info, paint_offset);
    return;
  }

  // Note that for fieldsets we need to enter decoration and background painting
  // even if we have no such things, because the rendered legend is painted in
  // this phase as well. Hence the early check above.
  const ComputedStyle& style = box_fragment_.Style();
  if (!style.HasBoxDecorationBackground())
    return;

  // TODO(mstensho): Break dependency on LayoutObject functionality.
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();

  PhysicalRect paint_rect;
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  if (IsPaintingScrollingBackground(box_fragment_, paint_info)) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    const LayoutBox& layout_box = ToLayoutBox(layout_object);
    paint_rect = layout_box.PhysicalLayoutOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box);
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box.BorderBoxOutsets());
  } else {
    paint_rect.offset = paint_offset;
    paint_rect.size = box_fragment_.Size();
  }

  PaintBoxDecorationBackgroundWithRect(
      contents_paint_state ? contents_paint_state->GetPaintInfo() : paint_info,
      paint_rect);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
bool NGBoxFragmentPainter::BackgroundIsKnownToBeOpaque(
    const PaintInfo& paint_info) {
  const LayoutBox& layout_box = ToLayoutBox(*box_fragment_.GetLayoutObject());

  // If the box has multiple fragments, its VisualRect is the bounding box of
  // all fragments' visual rects, which is likely to cover areas that are not
  // covered by painted background.
  if (layout_box.FirstFragment().NextFragment())
    return false;

  PhysicalRect bounds = IsPaintingScrollingBackground(box_fragment_, paint_info)
                            ? layout_box.PhysicalLayoutOverflowRect()
                            : layout_box.PhysicalSelfVisualOverflowRect();
  return layout_box.BackgroundIsKnownToBeOpaqueInRect(bounds);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = ToLayoutBox(layout_object);

  const ComputedStyle& style = box_fragment_.Style();

  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data, and for
  // delayed-invalidation object because it may change before it's actually
  // invalidated. Note that we still report harmless under-invalidation of
  // non-delayed-invalidation animated background, which should be ignored.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      (style.Appearance() == kMediaSliderPart ||
       layout_box.ShouldDelayFullPaintInvalidation())) {
    cache_skipper.emplace(paint_info.context);
  }

  BoxDecorationData box_decoration_data(paint_info, PhysicalFragment());
  if (!box_decoration_data.ShouldPaint())
    return;

  const DisplayItemClient& display_item_client =
      box_decoration_data.IsPaintingScrollingBackground()
          ? layout_box.GetScrollableArea()
                ->GetScrollingBackgroundDisplayItemClient()
          : box_fragment_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      paint_rect.EdgesOnPixelBoundaries() &&
      BackgroundIsKnownToBeOpaque(paint_info))
    recorder.SetKnownToBeOpaque();

  const NGBorderEdges& border_edges = BorderEdges();
  if (box_decoration_data.ShouldPaintShadow()) {
    PaintNormalBoxShadow(paint_info, paint_rect, style, border_edges.line_left,
                         border_edges.line_right,
                         !box_decoration_data.ShouldPaintBackground());
  }

  bool needs_end_layer = false;
  if (!box_decoration_data.IsPaintingScrollingBackground()) {
    if (box_fragment_.HasSelfPaintingLayer() && layout_box.IsTableCell() &&
        ToLayoutTableCell(layout_box).Table()->ShouldCollapseBorders()) {
      // We have to clip here because the background would paint on top of the
      // collapsed table borders otherwise, since this is a self-painting layer.
      PhysicalRect clip_rect = paint_rect;
      clip_rect.Expand(ToLayoutTableCell(layout_box).BorderInsets());
      state_saver.Save();
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    } else if (BleedAvoidanceIsClipping(
                   box_decoration_data.GetBackgroundBleedAvoidance())) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(
          paint_rect.ToLayoutRect(), border_edges.line_left,
          border_edges.line_right);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.GetBackgroundBleedAvoidance() ==
          kBackgroundBleedClipLayer) {
        paint_info.context.BeginLayer();
        needs_end_layer = true;
      }
    }
  }

  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.HasAppearance() &&
      !theme_painter.Paint(layout_box, paint_info, snapped_paint_rect);
  if (!theme_painted) {
    if (box_decoration_data.ShouldPaintBackground()) {
      PaintBackground(paint_info, paint_rect,
                      box_decoration_data.BackgroundColor(),
                      box_decoration_data.GetBackgroundBleedAvoidance());
    }
    if (box_decoration_data.HasAppearance()) {
      theme_painter.PaintDecorations(layout_box.GetNode(),
                                     layout_box.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (box_decoration_data.ShouldPaintShadow()) {
    PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style,
                                      border_edges.line_left,
                                      border_edges.line_right);
  }

    // The theme will tell us whether or not we should also paint the CSS
    // border.
  if (box_decoration_data.ShouldPaintBorder()) {
    if (!theme_painted) {
      theme_painted =
          box_decoration_data.HasAppearance() &&
          !LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box.GetNode(), style, paint_info, snapped_paint_rect);
    }
    if (!theme_painted) {
      Node* generating_node = layout_object.GeneratingNode();
      const Document& document = layout_object.GetDocument();
      PaintBorder(*box_fragment_.GetLayoutObject(), document, generating_node,
                  paint_info, paint_rect, style,
                  box_decoration_data.GetBackgroundBleedAvoidance(),
                  border_edges.line_left, border_edges.line_right);
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = ToLayoutBox(layout_object);
  if (layout_box.BackgroundTransfersToView())
    return;
  if (layout_box.BackgroundIsKnownToBeObscured())
    return;
  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));
  PaintFillLayers(paint_info, background_color,
                  box_fragment_.Style().BackgroundLayers(), paint_rect,
                  geometry, bleed_avoidance);
}

void NGBoxFragmentPainter::PaintInlineChildBoxUsingLegacyFallback(
    const NGPhysicalFragment& fragment,
    const PaintInfo& paint_info) {
  const LayoutObject* child_layout_object = fragment.GetLayoutObject();
  DCHECK(child_layout_object);
  if (child_layout_object->PaintFragment()) {
    // This object will use NGBoxFragmentPainter.
    child_layout_object->Paint(paint_info);
    return;
  }

  if (child_layout_object->IsAtomicInlineLevel()) {
    // Pre-NG painters also expect callers to use |PaintAllPhasesAtomically()|
    // for atomic inlines.
    ObjectPainter(*child_layout_object).PaintAllPhasesAtomically(paint_info);
    return;
  }

  child_layout_object->Paint(paint_info);
}

void NGBoxFragmentPainter::PaintAllPhasesAtomically(
    const PaintInfo& paint_info) {
  // Self-painting AtomicInlines should go to normal paint logic.
  DCHECK(!(PhysicalFragment().IsAtomicInline() &&
           box_fragment_.HasSelfPaintingLayer()));

  // Pass PaintPhaseSelection and PaintPhaseTextClip is handled by the regular
  // foreground paint implementation. We don't need complete painting for these
  // phases.
  PaintPhase phase = paint_info.phase;
  if (phase == PaintPhase::kSelection || phase == PaintPhase::kTextClip)
    return PaintInternal(paint_info);

  if (phase != PaintPhase::kForeground)
    return;

  PaintInfo local_paint_info(paint_info);
  local_paint_info.phase = PaintPhase::kBlockBackground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kFloat;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kForeground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kOutline;
  PaintInternal(local_paint_info);
}

void NGBoxFragmentPainter::PaintLineBoxChildren(
    NGPaintFragment::ChildList line_boxes,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask &&
      paint_info.phase != PaintPhase::kDescendantOutlinesOnly &&
      paint_info.phase != PaintPhase::kOutline)
    return;

  // The only way an inline could paint like this is if it has a layer.
  const auto* layout_object = box_fragment_.GetLayoutObject();
  DCHECK(layout_object->IsLayoutBlock() ||
         (layout_object->IsLayoutInline() && layout_object->HasLayer()));

  // if (paint_info.phase == PaintPhase::kForeground && paint_info.IsPrinting())
  //  AddPDFURLRectsForInlineChildrenRecursively(layout_object, paint_info,
  //                                             paint_offset);

  // If we have no lines then we have no work to do.
  if (line_boxes.IsEmpty())
    return;

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  const auto& layout_block = To<LayoutBlock>(*layout_object);
  if (RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled() ||
      RuntimeEnabledFeatures::ElementTimingEnabled(
          &layout_block.GetDocument())) {
    if (paint_info.phase == PaintPhase::kForeground) {
      scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
          layout_block, paint_info.context.GetPaintController()
                            .CurrentPaintChunkProperties());
    }
  }

  const bool is_horizontal = box_fragment_.Style().IsHorizontalWritingMode();

  for (const NGPaintFragment* line : line_boxes) {
    const NGPhysicalFragment& child_fragment = line->PhysicalFragment();
    DCHECK(!child_fragment.IsOutOfFlowPositioned());
    if (child_fragment.IsFloating())
      continue;

    // Check if CullRect intersects with this child, only in block direction
    // because soft-wrap and <br> needs to paint outside of InkOverflow() in
    // inline direction.
    const PhysicalOffset child_offset = paint_offset + line->Offset();
    PhysicalRect child_rect = line->InkOverflow();
    if (is_horizontal) {
      LayoutUnit y = child_rect.offset.top + child_offset.top;
      if (!paint_info.GetCullRect().IntersectsVerticalRange(
              y, y + child_rect.size.height))
        continue;
    } else {
      LayoutUnit x = child_rect.offset.left + child_offset.left;
      if (!paint_info.GetCullRect().IntersectsHorizontalRange(
              x, x + child_rect.size.width))
        continue;
    }

    if (child_fragment.IsListMarker()) {
      PaintAtomicInlineChild(*line, paint_info);
      continue;
    }
    DCHECK(child_fragment.IsLineBox());

    if (paint_info.phase == PaintPhase::kForeground) {
      if (NGFragmentPainter::ShouldRecordHitTestData(paint_info,
                                                     PhysicalFragment())) {
        RecordHitTestDataForLine(paint_info, child_offset, *line);
      }

      // Line boxes don't paint anything, except when its ::first-line style has
      // a background.
      if (UNLIKELY(NGLineBoxFragmentPainter::NeedsPaint(child_fragment))) {
        NGLineBoxFragmentPainter line_box_painter(*line, box_fragment_);
        line_box_painter.PaintBackgroundBorderShadow(paint_info, child_offset);
      }
    }

    PaintInlineChildren(line->Children(), paint_info, child_offset);
  }
}

void NGBoxFragmentPainter::PaintInlineChildren(
    NGPaintFragment::ChildList inline_children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  // TODO(kojii): Move kOutline painting into a |PaintInlineChildrenOutlines()|
  // method instead as it would be more efficient. Would require repeating some
  // of the code below though.
  // This DCHECK can then match to |InlineFlowBoxPainter::Paint|.
  DCHECK_NE(paint_info.phase, PaintPhase::kDescendantOutlinesOnly);

  for (const NGPaintFragment* child : inline_children) {
    const NGPhysicalFragment& child_fragment = child->PhysicalFragment();
    if (child_fragment.IsHiddenForPaint())
      continue;
    if (child_fragment.IsFloating())
      continue;

    // Skip if this child does not intersect with CullRect.
    if (!paint_info.IntersectsCullRect(child->InkOverflow(),
                                       paint_offset + child->Offset()) &&
        // Don't skip empty size text in order to paint selection for <br>.
        !(child_fragment.IsText() && child_fragment.Size().IsEmpty()))
      continue;

    if (child_fragment.Type() == NGPhysicalFragment::kFragmentText) {
      DCHECK(!child_fragment.HasSelfPaintingLayer());
      PaintTextChild(*child, paint_info, paint_offset);
    } else if (child_fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (child_fragment.HasSelfPaintingLayer())
        continue;
      if (child_fragment.IsAtomicInline())
        PaintAtomicInlineChild(*child, paint_info);
      else
        NGInlineBoxFragmentPainter(*child).Paint(paint_info, paint_offset);
    } else {
      NOTREACHED();
    }
  }
}

void NGBoxFragmentPainter::PaintAtomicInlineChild(const NGPaintFragment& child,
                                                  const PaintInfo& paint_info) {
  // Inline children should be painted by PaintInlineChild.
  DCHECK(child.PhysicalFragment().IsAtomicInline());

  const NGPhysicalFragment& fragment = child.PhysicalFragment();
  if (child.HasSelfPaintingLayer())
    return;
  if (fragment.Type() == NGPhysicalFragment::kFragmentBox &&
      FragmentRequiresLegacyFallback(fragment)) {
    PaintInlineChildBoxUsingLegacyFallback(fragment, paint_info);
  } else {
    NGBoxFragmentPainter(child).PaintAllPhasesAtomically(paint_info);
  }
}

void NGBoxFragmentPainter::PaintTextChild(const NGPaintFragment& paint_fragment,
                                          const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  // Inline blocks should be painted by PaintAtomicInlineChild.
  DCHECK(!paint_fragment.PhysicalFragment().IsAtomicInline());

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  const auto& text_fragment =
      To<NGPhysicalTextFragment>(paint_fragment.PhysicalFragment());
  DOMNodeId node_id = kInvalidDOMNodeId;
  if (auto* node = text_fragment.GetNode()) {
    if (node->GetLayoutObject()->IsText())
      node_id = ToLayoutText(node->GetLayoutObject())->EnsureNodeId();
  }

  NGTextFragmentPainter text_painter(paint_fragment);
  text_painter.Paint(paint_info, paint_offset, node_id);
}

void NGBoxFragmentPainter::PaintAtomicInline(const PaintInfo& paint_info) {
  DCHECK(PhysicalFragment().IsAtomicInline());
  // Self-painting AtomicInlines should go to normal paint logic.
  DCHECK(!box_fragment_.HasSelfPaintingLayer());

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  if (paint_info.phase == PaintPhase::kTextClip)
    return;

  PaintAllPhasesAtomically(paint_info);
}

bool NGBoxFragmentPainter::IsPaintingScrollingBackground(
    const NGPaintFragment& fragment,
    const PaintInfo& paint_info) {
  // TODO(layout-dev): Change paint_info.PaintContainer to accept fragments
  // once LayoutNG supports scrolling containers.
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         box_fragment_.GetLayoutObject() == paint_info.PaintContainer();
}

bool NGBoxFragmentPainter::ShouldPaint(
    const ScopedPaintState& paint_state) const {
  // TODO(layout-dev): Add support for scrolling, see BlockPainter::ShouldPaint.
  return paint_state.LocalRectIntersectsCullRect(box_fragment_.InkOverflow());
}

void NGBoxFragmentPainter::PaintTextClipMask(GraphicsContext& context,
                                             const IntRect& mask_rect,
                                             const PhysicalOffset& paint_offset,
                                             bool object_has_multiple_boxes) {
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (object_has_multiple_boxes) {
    PhysicalOffset local_offset = box_fragment_.Offset();
    NGInlineBoxFragmentPainter inline_box_painter(box_fragment_);
    if (box_fragment_.Style().BoxDecorationBreak() ==
        EBoxDecorationBreak::kSlice) {
      LayoutUnit offset_on_line;
      LayoutUnit total_width;
      inline_box_painter.ComputeFragmentOffsetOnLine(
          box_fragment_.Style().Direction(), &offset_on_line, &total_width);
      LayoutSize line_offset(offset_on_line, LayoutUnit());
      local_offset -=
          PhysicalOffset(box_fragment_.Style().IsHorizontalWritingMode()
                             ? line_offset
                             : line_offset.TransposedSize());
    }
    inline_box_painter.Paint(paint_info, paint_offset - local_offset);
  } else {
    PaintObject(paint_info, paint_offset);
  }
}

PhysicalRect NGBoxFragmentPainter::AdjustRectForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const PhysicalRect& rect) {
  PhysicalRect scrolled_paint_rect = rect;
  GraphicsContext& context = paint_info.context;
  const NGPhysicalBoxFragment& physical = PhysicalFragment();

  // Clip to the overflow area.
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingScrollingBackground(box_fragment_, paint_info)) {
    context.Clip(FloatRect(physical.OverflowClipRect(rect.offset)));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    PhysicalOffset offset(physical.ScrolledContentOffset());
    scrolled_paint_rect.Move(-offset);
    LayoutRectOutsets borders = AdjustedBorderOutsets(info);
    scrolled_paint_rect.size =
        physical.ScrollSize() + PhysicalSize(borders.Size());
  }
  return scrolled_paint_rect;
}

LayoutRectOutsets NGBoxFragmentPainter::ComputeBorders() const {
  return BoxStrutToLayoutRectOutsets(
      To<NGPhysicalBoxFragment>(box_fragment_.PhysicalFragment())
          .BorderWidths());
}

LayoutRectOutsets NGBoxFragmentPainter::ComputePadding() const {
  return BoxStrutToLayoutRectOutsets(
      To<NGPhysicalBoxFragment>(box_fragment_.PhysicalFragment())
          .PixelSnappedPadding());
}

BoxPainterBase::FillLayerInfo NGBoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance) const {
  const NGBorderEdges& border_edges = BorderEdges();
  return BoxPainterBase::FillLayerInfo(
      box_fragment_.GetLayoutObject()->GetDocument(), box_fragment_.Style(),
      box_fragment_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      border_edges.line_left, border_edges.line_right);
}

bool NGBoxFragmentPainter::IsInSelfHitTestingPhase(HitTestAction action) const {
  // TODO(layout-dev): We should set an IsContainingBlock flag on
  // NGPhysicalBoxFragment, instead of routing back to LayoutObject.
  const LayoutObject* layout_object = box_fragment_.GetLayoutObject();
  if (layout_object->IsBox())
    return ToLayoutBox(layout_object)->IsInSelfHitTestingPhase(action);
  return action == kHitTestForeground;
}

bool NGBoxFragmentPainter::NodeAtPoint(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& physical_offset,
                                       HitTestAction action) {
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  const PhysicalSize& size = box_fragment_.Size();
  const ComputedStyle& style = box_fragment_.Style();

  bool hit_test_self = IsInSelfHitTestingPhase(action);

  // TODO(layout-dev): Add support for hit testing overflow controls once we
  // overflow has been implemented.
  // if (hit_test_self && HasOverflowClip() &&
  //   HitTestOverflowControl(result, hit_test_location, physical_offset))
  // return true;

  bool skip_children = result.GetHitTestRequest().GetStopNode() ==
                       PhysicalFragment().GetLayoutObject();
  if (!skip_children && box_fragment_.ShouldClipOverflow()) {
    // PaintLayer::HitTestContentsForFragments checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!box_fragment_.HasSelfPaintingLayer() &&
        !hit_test_location.Intersects(PhysicalFragment().OverflowClipRect(
            physical_offset, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && style.HasBorderRadius()) {
      PhysicalRect bounds_rect(physical_offset, size);
      skip_children = !hit_test_location.Intersects(
          style.GetRoundedInnerBorderFor(bounds_rect.ToLayoutRect()));
    }
  }

  if (!skip_children) {
    PhysicalOffset scrolled_offset = physical_offset;
    if (box_fragment_.HasOverflowClip()) {
      scrolled_offset -=
          PhysicalOffset(PhysicalFragment().ScrolledContentOffset());
    }
    if (HitTestChildren(result, box_fragment_.Children(), hit_test_location,
                        scrolled_offset, action)) {
      return true;
    }
  }

  if (style.HasBorderRadius() &&
      HitTestClippedOutByBorder(hit_test_location, physical_offset))
    return false;

  // Now hit test ourselves.
  if (hit_test_self && VisibleToHitTestRequest(result.GetHitTestRequest())) {
    PhysicalRect bounds_rect(physical_offset, size);
    if (UNLIKELY(result.GetHitTestRequest().GetType() &
                 HitTestRequest::kHitTestVisualOverflow)) {
      bounds_rect = box_fragment_.SelfInkOverflow();
      bounds_rect.Move(physical_offset);
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    if (fragment.IsInlineBox())
      bounds_rect = PhysicalRect(PixelSnappedIntRect(bounds_rect));
    if (hit_test_location.Intersects(bounds_rect)) {
      Node* node = box_fragment_.NodeForHitTest();
      if (!result.InnerNode() && node) {
        PhysicalOffset point = hit_test_location.Point() - physical_offset;
        result.SetNodeAndPosition(node, point);
      }
      if (result.AddNodeToListBasedTestResult(node, hit_test_location,
                                              bounds_rect) == kStopHitTesting) {
        return true;
      }
    }
  }

  return false;
}

bool NGBoxFragmentPainter::VisibleToHitTestRequest(
    const HitTestRequest& request) const {
  return FragmentVisibleToHitTestRequest(box_fragment_, request);
}

bool NGBoxFragmentPainter::HitTestTextFragment(
    HitTestResult& result,
    const NGPaintFragment& text_paint_fragment,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset,
    HitTestAction action) {
  if (action != kHitTestForeground)
    return false;

  const auto& text_fragment =
      To<NGPhysicalTextFragment>(text_paint_fragment.PhysicalFragment());
  PhysicalSize size(text_fragment.Size().width, text_fragment.Size().height);
  PhysicalRect border_rect(physical_offset, size);

  // TODO(layout-dev): Clip to line-top/bottom.
  PhysicalRect rect(PixelSnappedIntRect(border_rect));
  if (UNLIKELY(result.GetHitTestRequest().GetType() &
               HitTestRequest::kHitTestVisualOverflow)) {
    rect = text_fragment.SelfInkOverflow();
    rect.Move(border_rect.offset);
  }

  if (FragmentVisibleToHitTestRequest(text_paint_fragment,
                                      result.GetHitTestRequest()) &&
      hit_test_location.Intersects(rect)) {
    Node* node = text_paint_fragment.NodeForHitTest();
    if (!result.InnerNode() && node) {
      PhysicalOffset point = hit_test_location.Point() - physical_offset +
                             text_paint_fragment.InlineOffsetToContainerBox();
      result.SetNodeAndPosition(node, point);
    }

    if (result.AddNodeToListBasedTestResult(node, hit_test_location, rect) ==
        kStopHitTesting) {
      return true;
    }
  }

  return false;
}

// Replicates logic in legacy InlineFlowBox::NodeAtPoint().
bool NGBoxFragmentPainter::HitTestLineBoxFragment(
    HitTestResult& result,
    const NGPaintFragment& fragment,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset,
    HitTestAction action) {
  if (HitTestChildren(result, fragment.Children(), hit_test_location,
                      physical_offset, action))
    return true;

  if (action != kHitTestForeground)
    return false;

  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  const PhysicalOffset overflow_location =
      fragment.SelfInkOverflow().offset + physical_offset;
  if (HitTestClippedOutByBorder(hit_test_location, overflow_location))
    return false;

  const PhysicalSize size = fragment.Size();
  const PhysicalRect bounds_rect(physical_offset, size);
  const ComputedStyle& containing_box_style = box_fragment_.Style();
  if (containing_box_style.HasBorderRadius() &&
      !hit_test_location.Intersects(containing_box_style.GetRoundedBorderFor(
          bounds_rect.ToLayoutRect()))) {
    return false;
  }

  // Now hit test ourselves.
  if (!hit_test_location.Intersects(bounds_rect))
    return false;

  // Floats will be hit-tested in |kHitTestFloat| phase, but
  // |LayoutObject::HitTestAllPhases| does not try it if |kHitTestForeground|
  // succeeds. Pretend the location is not in this linebox if it hits floating
  // descendants. TODO(kojii): Computing this is redundant, consider
  // restructuring. Changing the caller logic isn't easy because currently
  // floats are in the bounds of line boxes only in NG.
  const auto& line = To<NGPhysicalLineBoxFragment>(fragment.PhysicalFragment());
  if (line.HasFloatingDescendants()) {
    DCHECK_NE(action, kHitTestFloat);
    if (HitTestChildren(result, fragment.Children(), hit_test_location,
                        physical_offset, kHitTestFloat)) {
      return false;
    }
  }

  Node* node = fragment.NodeForHitTest();
  if (!result.InnerNode() && node) {
    const PhysicalOffset point = hit_test_location.Point() - physical_offset +
                                 fragment.InlineOffsetToContainerBox();
    result.SetNodeAndPosition(node, point);
  }
  return result.AddNodeToListBasedTestResult(node, hit_test_location,
                                             bounds_rect) == kStopHitTesting;
}

bool NGBoxFragmentPainter::HitTestChildBoxFragment(
    HitTestResult& result,
    const NGPaintFragment& paint_fragment,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& physical_offset,
    HitTestAction action) {
  const NGPhysicalFragment& fragment = paint_fragment.PhysicalFragment();

  // Note: Floats should only be hit tested in the |kHitTestFloat| phase, so we
  // shouldn't enter a float when |action| doesn't match. However, as floats may
  // scatter around in the entire inline formatting context, we should always
  // enter non-floating inline child boxes to search for floats in the
  // |kHitTestFloat| phase, unless the child box forms another context.

  if (fragment.IsFloating() && action != kHitTestFloat)
    return false;

  if (!FragmentRequiresLegacyFallback(fragment)) {
    // TODO(layout-dev): Implement HitTestAllPhases in NG after we stop
    // falling back to legacy for child atomic inlines and floats.
    DCHECK(!fragment.IsAtomicInline());
    DCHECK(!fragment.IsFloating());
    return NGBoxFragmentPainter(paint_fragment)
        .NodeAtPoint(result, hit_test_location, physical_offset, action);
  }

  if (fragment.IsInline() && action != kHitTestForeground)
    return false;

  LayoutBox* const layout_box = ToLayoutBox(fragment.GetMutableLayoutObject());

  // https://www.w3.org/TR/CSS22/zindex.html#painting-order
  // Hit test all phases of inline blocks, inline tables, replaced elements and
  // non-positioned floats as if they created their own stacking contexts.
  const bool should_hit_test_all_phases =
      fragment.IsAtomicInline() || fragment.IsFloating();
  return should_hit_test_all_phases
             ? layout_box->HitTestAllPhases(result, hit_test_location,
                                            physical_offset)
             : layout_box->NodeAtPoint(result, hit_test_location,
                                       physical_offset, action);
}

bool NGBoxFragmentPainter::HitTestChildren(
    HitTestResult& result,
    NGPaintFragment::ChildList children,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  Vector<NGPaintFragment*, 16> child_vector;
  children.ToList(&child_vector);
  for (unsigned i = child_vector.size(); i;) {
    const NGPaintFragment* child = child_vector[--i];
    const PhysicalOffset offset = child->Offset();
    if (child->HasSelfPaintingLayer())
      continue;

    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    const PhysicalOffset child_physical_offset = accumulated_offset + offset;

    bool stop_hit_testing = false;
    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      stop_hit_testing = HitTestChildBoxFragment(
          result, *child, hit_test_location, child_physical_offset, action);

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentLineBox) {
      stop_hit_testing = HitTestLineBoxFragment(
          result, *child, hit_test_location, child_physical_offset, action);

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentText) {
      stop_hit_testing = HitTestTextFragment(result, *child, hit_test_location,
                                             child_physical_offset, action);
    }
    if (stop_hit_testing)
      return true;

    if (!fragment.IsInline() || action != kHitTestForeground)
      continue;

    // Hit test culled inline boxes between |fragment| and its parent fragment.
    const NGPaintFragment* previous_sibling = i ? child_vector[i - 1] : nullptr;
    if (HitTestCulledInlineAncestors(result, *child, previous_sibling,
                                     hit_test_location, child_physical_offset))
      return true;
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestClippedOutByBorder(
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& border_box_location) const {
  const ComputedStyle& style = box_fragment_.Style();
  PhysicalRect rect(PhysicalOffset(), PhysicalFragment().Size());
  rect.Move(border_box_location);
  const NGBorderEdges& border_edges = BorderEdges();
  return !hit_test_location.Intersects(style.GetRoundedBorderFor(
      rect.ToLayoutRect(), border_edges.line_left, border_edges.line_right));
}

}  // namespace blink
