// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

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

}  // namespace blink
