// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// TODO(layout-dev): Using ScrollableOverflow() is same as legacy
// LayoutRubyRun. However its result is not good with some fonts/platforms.
// See crbug.com/1082087.
LayoutUnit LastLineTextLogicalBottom(const NGPhysicalBoxFragment& container,
                                     LayoutUnit default_value) {
  const ComputedStyle& container_style = container.Style();
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    if (!container.Items())
      return default_value;
    NGInlineCursor cursor(*container.Items());
    cursor.MoveToLastLine();
    const auto* line_item = cursor.CurrentItem();
    if (!line_item)
      return default_value;
    DCHECK_EQ(line_item->Type(), NGFragmentItem::kLine);
    DCHECK(line_item->LineBoxFragment());
    PhysicalRect line_rect =
        line_item->LineBoxFragment()->ScrollableOverflowForLine(
            container, container_style, *line_item, cursor);
    return container.ConvertChildToLogical(line_rect).BlockEndOffset();
  }

  const NGPhysicalLineBoxFragment* last_line = nullptr;
  PhysicalOffset last_line_offset;
  for (const auto& child_link : container.PostLayoutChildren()) {
    if (const auto* maybe_line =
            DynamicTo<NGPhysicalLineBoxFragment>(*child_link)) {
      last_line = maybe_line;
      last_line_offset = child_link.offset;
    }
  }
  if (!last_line)
    return default_value;
  PhysicalRect line_rect =
      last_line->ScrollableOverflow(container, container_style);
  line_rect.Move(last_line_offset);
  return container.ConvertChildToLogical(line_rect).BlockEndOffset();
}

// TODO(layout-dev): Using ScrollableOverflow() is same as legacy
// LayoutRubyRun. However its result is not good with some fonts/platforms.
// See crbug.com/1082087.
LayoutUnit FirstLineTextLogicalTop(const NGPhysicalBoxFragment& container,
                                   LayoutUnit default_value) {
  const ComputedStyle& container_style = container.Style();
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    if (!container.Items())
      return default_value;
    NGInlineCursor cursor(*container.Items());
    cursor.MoveToFirstLine();
    const auto* line_item = cursor.CurrentItem();
    if (!line_item)
      return default_value;
    DCHECK_EQ(line_item->Type(), NGFragmentItem::kLine);
    DCHECK(line_item->LineBoxFragment());
    PhysicalRect line_rect =
        line_item->LineBoxFragment()->ScrollableOverflowForLine(
            container, container_style, *line_item, cursor);
    return container.ConvertChildToLogical(line_rect).offset.block_offset;
  }

  for (const auto& child_link : container.PostLayoutChildren()) {
    if (const auto* line = DynamicTo<NGPhysicalLineBoxFragment>(*child_link)) {
      PhysicalRect line_rect =
          line->ScrollableOverflow(container, container_style);
      line_rect.Move(child_link.offset);
      return container.ConvertChildToLogical(line_rect).offset.block_offset;
    }
  }
  return default_value;
}

// See LayoutRubyRun::GetOverhang().
NGAnnotationOverhang GetOverhang(const NGInlineItemResult& item) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGRubyEnabled());
  NGAnnotationOverhang overhang;
  if (!item.layout_result)
    return overhang;

  const auto& run_fragment =
      To<NGPhysicalContainerFragment>(item.layout_result->PhysicalFragment());
  LayoutUnit start_overhang = LayoutUnit::Max();
  LayoutUnit end_overhang = LayoutUnit::Max();
  bool found_line = false;
  const ComputedStyle* ruby_text_style = nullptr;
  for (const auto& child_link : run_fragment.PostLayoutChildren()) {
    const NGPhysicalFragment& child_fragment = *child_link.get();
    const LayoutObject* layout_object = child_fragment.GetLayoutObject();
    if (!layout_object)
      continue;
    if (layout_object->IsRubyText()) {
      ruby_text_style = layout_object->Style();
      continue;
    }
    if (layout_object->IsRubyBase()) {
      const ComputedStyle& base_style = child_fragment.Style();
      const WritingMode writing_mode = base_style.GetWritingMode();
      const LayoutUnit base_inline_size =
          NGFragment(writing_mode, child_fragment).InlineSize();
      // RubyBase's inline_size is always same as RubyRun's inline_size.
      // Overhang values are offsets from RubyBase's inline edges to
      // the outmost text.
      for (const auto& base_child_link :
           To<NGPhysicalContainerFragment>(child_fragment)
               .PostLayoutChildren()) {
        const LayoutUnit line_inline_size =
            NGFragment(writing_mode, *base_child_link).InlineSize();
        if (line_inline_size == LayoutUnit())
          continue;
        found_line = true;
        const LayoutUnit start =
            base_child_link.offset
                .ConvertToLogical(writing_mode, base_style.Direction(),
                                  child_fragment.Size(),
                                  base_child_link.get()->Size())
                .inline_offset;
        const LayoutUnit end = base_inline_size - start - line_inline_size;
        start_overhang = std::min(start_overhang, start);
        end_overhang = std::min(end_overhang, end);
      }
    }
  }

  if (!found_line || !ruby_text_style)
    return overhang;
  DCHECK_NE(start_overhang, LayoutUnit::Max());
  DCHECK_NE(end_overhang, LayoutUnit::Max());
  // We allow overhang up to the half of ruby text font size.
  const LayoutUnit half_width_of_ruby_font =
      LayoutUnit(ruby_text_style->FontSize()) / 2;
  overhang.start = std::min(start_overhang, half_width_of_ruby_font);
  overhang.end = std::min(end_overhang, half_width_of_ruby_font);
  return overhang;
}

// See LayoutRubyRun::GetOverhang().
bool CanApplyStartOverhang(const NGLineInfo& line_info,
                           LayoutUnit& start_overhang) {
  if (start_overhang <= LayoutUnit())
    return false;
  DCHECK(RuntimeEnabledFeatures::LayoutNGRubyEnabled());
  const NGInlineItemResults& items = line_info.Results();
  // Requires at least the current item and the previous item.
  if (items.size() < 2)
    return false;
  // Find a previous item other than kOpenTag/kCloseTag.
  // Searching items in the logical order doesn't work well with bidi
  // reordering. However, it's difficult to compute overhang after bidi
  // reordering because it affects line breaking.
  wtf_size_t previous_index = items.size() - 2;
  while ((items[previous_index].item->Type() == NGInlineItem::kOpenTag ||
          items[previous_index].item->Type() == NGInlineItem::kCloseTag) &&
         previous_index > 0)
    --previous_index;
  const NGInlineItemResult& previous_item = items[previous_index];
  if (previous_item.item->Type() != NGInlineItem::kText)
    return false;
  const NGInlineItem& current_item = *items.back().item;
  if (previous_item.item->Style()->FontSize() >
      current_item.Style()->FontSize())
    return false;
  start_overhang = std::min(start_overhang, previous_item.inline_size);
  return true;
}

// See LayoutRubyRun::GetOverhang().
LayoutUnit CommitPendingEndOverhang(NGLineInfo* line_info) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGRubyEnabled());
  DCHECK(line_info);
  NGInlineItemResults* items = line_info->MutableResults();
  if (items->size() < 2U)
    return LayoutUnit();
  const NGInlineItemResult& text_item = items->back();
  DCHECK_EQ(text_item.item->Type(), NGInlineItem::kText);
  wtf_size_t i = items->size() - 2;
  while ((*items)[i].item->Type() != NGInlineItem::kAtomicInline) {
    const auto type = (*items)[i].item->Type();
    if (type != NGInlineItem::kOpenTag && type != NGInlineItem::kCloseTag)
      return LayoutUnit();
    if (i-- == 0)
      return LayoutUnit();
  }
  NGInlineItemResult& atomic_inline_item = (*items)[i];
  if (!atomic_inline_item.layout_result->PhysicalFragment().IsRubyRun())
    return LayoutUnit();
  if (atomic_inline_item.pending_end_overhang <= LayoutUnit())
    return LayoutUnit();
  if (atomic_inline_item.item->Style()->FontSize() <
      text_item.item->Style()->FontSize())
    return LayoutUnit();
  // Ideally we should refer to inline_size of |text_item| instead of the width
  // of the NGInlineItem's ShapeResult. However it's impossible to compute
  // inline_size of |text_item| before calling BreakText(), and BreakText()
  // requires precise |position_| which takes |end_overhang| into account.
  LayoutUnit end_overhang =
      std::min(atomic_inline_item.pending_end_overhang,
               LayoutUnit(text_item.item->TextShapeResult()->Width()));
  DCHECK_EQ(atomic_inline_item.margins.inline_end, LayoutUnit());
  atomic_inline_item.margins.inline_end = -end_overhang;
  atomic_inline_item.inline_size -= end_overhang;
  atomic_inline_item.pending_end_overhang = LayoutUnit();
  return end_overhang;
}

NGAnnotationMetrics ComputeAnnotationOverflow(
    const NGLogicalLineItems& logical_line,
    const NGLineHeightMetrics& line_box_metrics,
    LayoutUnit line_over,
    const ComputedStyle& line_style) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGRubyEnabled());
  // Min/max position of content without line-height.
  LayoutUnit content_over = line_over + line_box_metrics.ascent;
  LayoutUnit content_under = content_over;

  // Min/max position of annotations.
  LayoutUnit annotation_over = content_over;
  LayoutUnit annotation_under = content_over;

  const LayoutUnit line_under = line_over + line_box_metrics.LineHeight();
  bool has_over_emphasis = false;
  bool has_under_emphasis = false;
  for (const NGLogicalLineItem& item : logical_line) {
    if (item.HasInFlowFragment()) {
      if (!item.IsControl()) {
        content_over = std::min(content_over, item.BlockOffset());
        content_under = std::max(content_under, item.BlockEndOffset());
      }
      if (const auto* style = item.Style()) {
        if (style->GetTextEmphasisMark() != TextEmphasisMark::kNone) {
          if (style->GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver)
            has_over_emphasis = true;
          else
            has_under_emphasis = true;
        }
      }
    }

    // Accumulate |AnnotationOverflow| from ruby runs. All ruby run items have
    // |layout_result|.
    const NGLayoutResult* layout_result = item.layout_result.get();
    if (!layout_result)
      continue;
    LayoutUnit overflow = layout_result->AnnotationOverflow();
    if (IsFlippedLinesWritingMode(line_style.GetWritingMode()))
      overflow = -overflow;
    if (overflow < LayoutUnit()) {
      annotation_over =
          std::min(annotation_over, item.rect.offset.block_offset + overflow);
    } else if (overflow > LayoutUnit()) {
      const LayoutUnit logical_bottom =
          item.rect.offset.block_offset +
          layout_result->PhysicalFragment()
              .Size()
              .ConvertToLogical(line_style.GetWritingMode())
              .block_size;
      annotation_under = std::max(annotation_under, logical_bottom + overflow);
    }
  }

  // Probably this is an empty line. We should secure font-size space.
  const LayoutUnit font_size(line_style.ComputedFontSize());
  if (content_under - content_over < font_size) {
    LayoutUnit half_leading = (line_box_metrics.LineHeight() - font_size) / 2;
    half_leading = half_leading.ClampNegativeToZero();
    content_over = line_over + half_leading;
    content_under = line_under - half_leading;
  }

  // Don't provide annotation space if text-emphasis exists.
  // TODO(layout-dev): If the text-emphasis is in [line_over, line_under],
  // this line can provide annotation space.
  if (has_over_emphasis)
    content_over = line_over;
  if (has_under_emphasis)
    content_under = line_under;

  const LayoutUnit overflow_over =
      (line_over - annotation_over).ClampNegativeToZero();
  const LayoutUnit overflow_under =
      (annotation_under - line_under).ClampNegativeToZero();
  return {overflow_over, overflow_under,
          // With some fonts, text fragment sizes can exceed line-height.
          // We need ClampNegativeToZero().
          overflow_over ? LayoutUnit()
                        : (content_over - line_over).ClampNegativeToZero(),
          overflow_under ? LayoutUnit()
                         : (line_under - content_under).ClampNegativeToZero()};
}

}  // namespace blink
