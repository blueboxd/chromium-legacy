// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               Color text_color) {
  const Color color =
      SelectionPaintingUtils::SelectionBackgroundColor(document, style, node);
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color)
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  return color;
}

DocumentMarkerVector ComputeMarkersToPaint(Node* node, bool is_ellipsis) {
  // TODO(yoichio): Handle first-letter
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return DocumentMarkerVector();
  // We don't paint any marker on ellipsis.
  if (is_ellipsis)
    return DocumentMarkerVector();

  DocumentMarkerController& document_marker_controller =
      node->GetDocument().Markers();
  return document_marker_controller.ComputeMarkersToPaint(*text_node);
}

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const base::Optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

// ClampOffset modifies |offset| fixed in a range of |text_fragment| start/end
// offsets.
// |offset| points not each character but each span between character.
// With that concept, we can clear catch what is inside start / end.
// Suppose we have "foo_bar"('_' is a space).
// There are 8 offsets for that:
//  f o o _ b a r
// 0 1 2 3 4 5 6 7
// If "bar" is a TextFragment. That start(), end() {4, 7} correspond this
// offset. If a marker has StartOffset / EndOffset as {2, 6},
// ClampOffset returns{ 4,6 }, which represents "ba" on "foo_bar".
unsigned ClampOffset(unsigned offset, const NGFragmentItem& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

void PaintRect(GraphicsContext& context,
               const PhysicalOffset& location,
               const PhysicalRect& rect,
               const Color color) {
  if (!color.Alpha())
    return;
  if (rect.size.IsEmpty())
    return;
  const IntRect pixel_snapped_rect =
      PixelSnappedIntRect(PhysicalRect(rect.offset + location, rect.size));
  if (!pixel_snapped_rect.IsEmpty())
    context.FillRect(pixel_snapped_rect, color);
}

PhysicalRect MarkerRectForForeground(const NGFragmentItem& text_fragment,
                                     StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      text_fragment.LineLeftAndRightForOffsets(text, start_offset, end_offset);

  const LayoutUnit height = text_fragment.Size()
                                .ConvertToLogical(static_cast<WritingMode>(
                                    text_fragment.Style().GetWritingMode()))
                                .block_size;
  return {start_position, LayoutUnit(), end_position - start_position, height};
}

// Copied from InlineTextBoxPainter
void PaintDocumentMarkers(GraphicsContext& context,
                          const NGFragmentItem& text_fragment,
                          StringView text,
                          const DocumentMarkerVector& markers_to_paint,
                          const PhysicalOffset& box_origin,
                          const ComputedStyle& style,
                          DocumentMarkerPaintPhase marker_paint_phase,
                          NGTextPainter* text_painter) {
  if (markers_to_paint.IsEmpty())
    return;

  DCHECK(text_fragment.GetNode());
  const auto& text_node = To<Text>(*text_fragment.GetNode());
  for (const DocumentMarker* marker : markers_to_paint) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text_node, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text_node, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, text_fragment);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, text_fragment);
    if (paint_start_offset == paint_end_offset)
      continue;

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (context.Printing())
          break;
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        DocumentMarkerPainter::PaintDocumentMarker(
            context, box_origin, style, marker->GetType(),
            MarkerRectForForeground(text_fragment, text, paint_start_offset,
                                    paint_end_offset));
      } break;

      case DocumentMarker::kTextMatch: {
        if (!text_fragment.GetNode()
                 ->GetDocument()
                 .GetFrame()
                 ->GetEditor()
                 .MarkedTextMatchesAreHighlighted())
          break;
        const auto& text_match_marker = To<TextMatchMarker>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          const Color color =
              LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                  text_match_marker.IsActiveMatch(),
                  text_fragment.GetNode()->GetDocument().InForcedColorsMode(),
                  style.UsedColorScheme());
          PaintRect(context, PhysicalOffset(box_origin),
                    text_fragment.LocalRect(text, paint_start_offset,
                                            paint_end_offset),
                    color);
          break;
        }

        const TextPaintStyle text_style =
            DocumentMarkerPainter::ComputeTextPaintStyleFrom(
                style, text_match_marker,
                text_fragment.GetNode()->GetDocument().InForcedColorsMode());
        if (text_style.current_color == Color::kTransparent)
          break;
        text_painter->Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style,
                            kInvalidDOMNodeId);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          PaintRect(context, PhysicalOffset(box_origin),
                    text_fragment.LocalRect(text, paint_start_offset,
                                            paint_end_offset),
                    styleable_marker.BackgroundColor());
          break;
        }
        const SimpleFontData* font_data = style.GetFont().PrimaryFont();
        DocumentMarkerPainter::PaintStyleableMarkerUnderline(
            context, box_origin, styleable_marker, style,
            FloatRect(MarkerRectForForeground(
                text_fragment, text, paint_start_offset, paint_end_offset)),
            LayoutUnit(font_data->GetFontMetrics().Height()));
      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace

NGTextFragmentPainter::NGTextFragmentPainter(
    const NGPaintFragment& paint_fragment)
    : paint_fragment_(&paint_fragment),
      text_fragment_(
          &To<NGPhysicalTextFragment>(paint_fragment.PhysicalFragment())) {}

NGTextFragmentPainter::NGTextFragmentPainter(const NGFragmentItem& item,
                                             const NGFragmentItems& items)
    : item_(&item), items_(&items) {
  DCHECK_EQ(item.Type(), NGFragmentItem::kText);
}

// Logic is copied from InlineTextBoxPainter::PaintSelection.
// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
static void PaintSelection(GraphicsContext& context,
                           const NGPaintFragment& paint_fragment,
                           Node* node,
                           const Document& document,
                           const ComputedStyle& style,
                           Color text_color,
                           const PhysicalRect& box_rect,
                           const LayoutSelectionStatus& selection_status) {
  const Color color =
      SelectionBackgroundColor(document, style, node, text_color);
  const PhysicalRect selection_rect =
      paint_fragment.ComputeLocalSelectionRectForText(selection_status);
  PaintRect(context, box_rect.offset, selection_rect, color);
}

void NGTextFragmentPainter::PaintSymbol(const LayoutObject* layout_object,
                                        const ComputedStyle& style,
                                        const PhysicalSize box_size,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  PhysicalRect marker_rect(
      LayoutListMarker::RelativeSymbolMarkerRect(style, box_size.width));
  marker_rect.Move(paint_offset);
  IntRect rect = PixelSnappedIntRect(marker_rect);

  ListMarkerPainter::PaintSymbol(paint_info, layout_object, style, rect);
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
void NGTextFragmentPainter::Paint(const PaintInfo& paint_info,
                                  const PhysicalOffset& paint_offset) {
  if (UNLIKELY(item_)) {
    PaintItem(paint_info, paint_offset);
    return;
  }
  DCHECK(text_fragment_ && paint_fragment_);

  // We can skip painting if the fragment (including selection) is invisible.
  if (!text_fragment_->Length())
    return;
  IntRect visual_rect = paint_fragment_->VisualRect();
  if (visual_rect.IsEmpty())
    return;

  if (!text_fragment_->TextShapeResult() &&
      // A line break's selection tint is still visible.
      !text_fragment_->IsLineBreak())
    return;

  Paint(text_fragment_->PaintInfo(), text_fragment_->GetLayoutObject(),
        *paint_fragment_, text_fragment_->Style(),
        {paint_fragment_->Offset(), text_fragment_->Size()}, visual_rect,
        text_fragment_->IsEllipsis(),
        text_fragment_->TextType() == NGPhysicalTextFragment::kSymbolMarker,
        paint_info, paint_offset);
}

void NGTextFragmentPainter::PaintItem(const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset) {
  DCHECK(item_ && items_);

  if (!item_->TextLength())
    return;
  IntRect visual_rect = item_->VisualRect();
  if (visual_rect.IsEmpty())
    return;

  NGTextFragmentPaintInfo fragment_paint_info = item_->TextPaintInfo(*items_);
  bool is_ellipsis = false;       // TODO(kojii): Implement.
  bool is_symbol_marker = false;  // TODO(kojii): Implement.
  Paint(fragment_paint_info, item_->GetLayoutObject(), *item_, item_->Style(),
        item_->Rect(), visual_rect, is_ellipsis, is_symbol_marker, paint_info,
        paint_offset);
}

void NGTextFragmentPainter::Paint(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const LayoutObject* layout_object,
    const DisplayItemClient& display_item_client,
    const ComputedStyle& style,
    PhysicalRect box_rect,
    const IntRect& visual_rect,
    bool is_ellipsis,
    bool is_symbol_marker,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK(layout_object);
  const Document& document = layout_object->GetDocument();
  const bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  bool have_selection = !is_printing &&
                        paint_info.phase != PaintPhase::kTextClip &&
                        layout_object->IsSelected();
  base::Optional<LayoutSelectionStatus> selection_status;
  if (have_selection) {
    if (paint_fragment_) {
      selection_status =
          document.GetFrame()->Selection().ComputeLayoutSelectionStatus(
              *paint_fragment_);
      DCHECK_LE(selection_status->start, selection_status->end);
      have_selection = selection_status->start < selection_status->end;
    } else {
      // TODO(kojii): Implement without paint_fragment_
      have_selection = false;
    }
  }
  if (!have_selection) {
    // When only painting the selection, don't bother to paint if there is none.
    if (paint_info.phase == PaintPhase::kSelection)
      return;

    // Flow controls (line break, tab, <wbr>) need only selection painting.
    if ((text_fragment_ && text_fragment_->IsFlowControl()) ||
        (item_ && item_->IsFlowControl()))
      return;
  }

  // The text clip phase already has a DrawingRecorder. Text clips are initiated
  // only in BoxPainterBase::PaintFillLayer, which is already within a
  // DrawingRecorder.
  base::Optional<DrawingRecorder> recorder;
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, display_item_client, paint_info.phase))
      return;
    recorder.emplace(paint_info.context, display_item_client, paint_info.phase);
  }

  if (UNLIKELY(is_symbol_marker)) {
    // The NGInlineItem of marker might be Split(). So PaintSymbol only if the
    // StartOffset is 0, or it might be painted several times.
    if (!fragment_paint_info.from) {
      PaintSymbol(layout_object, style, box_rect.size, paint_info,
                  paint_offset + box_rect.offset);
    }
    return;
  }

  // We round the y-axis to ensure consistent line heights.
  PhysicalOffset adjusted_paint_offset(paint_offset.left,
                                       LayoutUnit(paint_offset.top.Round()));
  box_rect.offset += adjusted_paint_offset;

  GraphicsContext& context = paint_info.context;

  // Determine text colors.

  Node* node = layout_object->GetNode();
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  TextPaintStyle selection_style = TextPainterBase::SelectionPaintingStyle(
      document, style, node, have_selection, paint_info, text_style);
  bool paint_selected_text_only = (paint_info.phase == PaintPhase::kSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  base::Optional<GraphicsContextStateSaver> state_saver;
  base::Optional<NGFragmentItem> text_item_buffer;
  if (text_fragment_)
    text_item_buffer.emplace(*text_fragment_);
  const NGFragmentItem& text_item =
      text_fragment_ ? text_item_buffer.value() : *item_;
  StringView text =
      text_fragment_ ? text_fragment_->Text() : item_->Text(*items_);

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  // Since NGPaintFragment::ComputeLocalSelectionRectForText() returns
  // PhysicalRect rather than LogicalRect, we should paint selection
  // before GraphicsContext flip.
  // TODO(yoichio): Make NGPhysicalTextFragment::LocalRect and
  // NGPaintFragment::ComputeLocalSelectionRectForText logical so that we can
  // paint selection in same fliped dimention as NGTextPainter.
  const DocumentMarkerVector& markers_to_paint =
      ComputeMarkersToPaint(node, is_ellipsis);
  if (paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing) {
    PaintDocumentMarkers(context, text_item, text, markers_to_paint,
                         box_rect.offset, style,
                         DocumentMarkerPaintPhase::kBackground, nullptr);
    if (have_selection) {
      if (paint_fragment_) {
        PaintSelection(context, *paint_fragment_, node, document, style,
                       selection_style.fill_color, box_rect, *selection_status);
      } else {
        // TODO(kojii): Implement without paint_fragment_.
      }
    }
  }

  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  if (!is_horizontal) {
    state_saver.emplace(context);
    // Because we rotate the GraphicsContext to match the logical direction,
    // transpose the |box_rect| to match to it.
    box_rect.size = PhysicalSize(box_rect.Height(), box_rect.Width());
    context.ConcatCTM(TextPainterBase::Rotation(
        box_rect, writing_mode != WritingMode::kSidewaysLr
                      ? TextPainterBase::kClockwise
                      : TextPainterBase::kCounterclockwise));
  }

  // 2. Now paint the foreground, including text and decorations.
  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  PhysicalOffset text_origin(box_rect.offset.left,
                             box_rect.offset.top + ascent);
  NGTextPainter text_painter(context, font, fragment_paint_info, visual_rect,
                             text_origin, box_rect, is_horizontal);

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = ToLayoutTextOrNull(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }

  const unsigned length = fragment_paint_info.to - fragment_paint_info.from;
  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style.TextDecorationsInEffect() != TextDecoration::kNone &&
        // Ellipsis should not have text decorations. This is not defined, but 4
        // impls do this.
        !is_ellipsis) {
      PhysicalOffset local_origin = box_rect.offset;
      LayoutUnit width = box_rect.Width();
      const NGPhysicalBoxFragment* decorating_box = nullptr;
      const ComputedStyle* decorating_box_style =
          decorating_box ? &decorating_box->Style() : nullptr;

      text_painter.ComputeDecorationInfo(
          decoration_info, box_rect.offset, local_origin, width,
          style.GetFontBaseline(), style, decorating_box_style);

      NGTextDecorationOffset decoration_offset(*decoration_info.style,
                                               text_item, decorating_box);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info, paint_info,
          style.AppliedTextDecorations(), text_style,
          &has_line_through_decoration);
    }

    unsigned start_offset = fragment_paint_info.from;
    unsigned end_offset = fragment_paint_info.to;

    if (have_selection && paint_selected_text_separately) {
      // Paint only the text that is not selected.
      if (start_offset < selection_status->start) {
        text_painter.Paint(start_offset, selection_status->start, length,
                           text_style, node_id);
      }
      if (selection_status->end < end_offset) {
        text_painter.Paint(selection_status->end, end_offset, length,
                           text_style, node_id);
      }
    } else {
      text_painter.Paint(start_offset, end_offset, length, text_style, node_id);
    }

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  if (have_selection &&
      (paint_selected_text_only || paint_selected_text_separately)) {
    // Paint only the text that is selected.
    text_painter.Paint(selection_status->start, selection_status->end, length,
                       selection_style, node_id);
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;
  PaintDocumentMarkers(context, text_item, text, markers_to_paint,
                       box_rect.offset, style,
                       DocumentMarkerPaintPhase::kForeground, &text_painter);
}

}  // namespace blink
