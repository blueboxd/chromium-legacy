// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(IsItemCursor());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  current_.item_iter_ = iter;
  current_.item_ = iter == items_.end() ? nullptr : iter->get();
}

void NGInlineCursor::SetRoot(const NGFragmentItems& fragment_items,
                             ItemsSpan items) {
  DCHECK(items.data() || !items.size());
  DCHECK(!HasRoot());
  fragment_items_ = &fragment_items;
  items_ = items;
  MoveToItem(items_.begin());
}

void NGInlineCursor::SetRoot(const NGFragmentItems& items) {
  SetRoot(items, items.Items());
}

void NGInlineCursor::SetRoot(const NGPaintFragment& root_paint_fragment) {
  DCHECK(&root_paint_fragment);
  DCHECK(!HasRoot());
  root_paint_fragment_ = &root_paint_fragment;
  current_.paint_fragment_ = root_paint_fragment.FirstChild();
}

void NGInlineCursor::SetRoot(const LayoutBlockFlow& block_flow) {
  DCHECK(&block_flow);
  DCHECK(!HasRoot());

  if (const NGPhysicalBoxFragment* fragment = block_flow.CurrentFragment()) {
    if (const NGFragmentItems* items = fragment->Items()) {
      SetRoot(*items);
      return;
    }
  }

  if (const NGPaintFragment* paint_fragment = block_flow.PaintFragment()) {
    SetRoot(*paint_fragment);
    return;
  }

  // We reach here in case of |ScrollANchor::NotifyBeforeLayout()| via
  // |LayoutText::PhysicalLinesBoundingBox()|
  // See external/wpt/css/css-scroll-anchoring/wrapped-text.html
}

NGInlineCursor::NGInlineCursor(const LayoutBlockFlow& block_flow) {
  SetRoot(block_flow);
}

NGInlineCursor::NGInlineCursor(const NGFragmentItems& fragment_items,
                               ItemsSpan items) {
  SetRoot(fragment_items, items);
}

NGInlineCursor::NGInlineCursor(const NGFragmentItems& items) {
  SetRoot(items);
}

NGInlineCursor::NGInlineCursor(const NGPaintFragment& root_paint_fragment) {
  SetRoot(root_paint_fragment);
}

bool NGInlineCursor::operator==(const NGInlineCursor& other) const {
  if (root_paint_fragment_) {
    return root_paint_fragment_ == other.root_paint_fragment_ &&
           current_.paint_fragment_ == other.current_.paint_fragment_;
  }
  if (current_.item_ != other.current_.item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(current_.item_iter_ == other.current_.item_iter_);
  return true;
}

const LayoutBlockFlow* NGInlineCursor::GetLayoutBlockFlow() const {
  if (IsPaintFragmentCursor()) {
    // |root_paint_fragment_| is either |LayoutBlockFlow| or |LayoutInline|.
    const NGPhysicalFragment& physical_fragment =
        root_paint_fragment_->PhysicalFragment();
    const LayoutObject* layout_object =
        physical_fragment.IsLineBox()
            ? To<NGPhysicalLineBoxFragment>(physical_fragment)
                  .ContainerLayoutObject()
            : physical_fragment.GetLayoutObject();
    if (const LayoutBlockFlow* block_flow =
            DynamicTo<LayoutBlockFlow>(layout_object))
      return block_flow;
    DCHECK(layout_object->IsLayoutInline());
    return layout_object->RootInlineFormattingContext();
  }
  if (IsItemCursor()) {
    for (const auto& item : items_) {
      const LayoutObject* layout_object = item->GetLayoutObject();
      if (layout_object && layout_object->IsInline())
        return layout_object->RootInlineFormattingContext();
    }
  }
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursor::HasChildren() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->FirstChild();
  if (current_.item_)
    return current_.item_->HasChildren();
  NOTREACHED();
  return false;
}

NGInlineCursor NGInlineCursor::CursorForDescendants() const {
  if (current_.paint_fragment_)
    return NGInlineCursor(*current_.paint_fragment_);
  if (current_.item_) {
    unsigned descendants_count = current_.item_->DescendantsCount();
    if (descendants_count > 1) {
      DCHECK(fragment_items_);
      return NGInlineCursor(
          *fragment_items_,
          ItemsSpan(&*(current_.item_iter_ + 1), descendants_count - 1));
    }
    return NGInlineCursor();
  }
  NOTREACHED();
  return NGInlineCursor();
}

void NGInlineCursor::ExpandRootToContainingBlock() {
  if (root_paint_fragment_) {
    root_paint_fragment_ = root_paint_fragment_->Root();
    return;
  }
  if (fragment_items_) {
    const unsigned index_diff = items_.data() - fragment_items_->Items().data();
    DCHECK_LT(index_diff, fragment_items_->Items().size());
    const unsigned item_index = current_.item_iter_ - items_.begin();
    items_ = fragment_items_->Items();
    // Update the iterator to the one for the new span.
    MoveToItem(items_.begin() + item_index + index_diff);
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::HasSoftWrapToNextLine() const {
  DCHECK(IsLineBox());
  const NGInlineBreakToken& break_token = CurrentInlineBreakToken();
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

bool NGInlineCursor::IsInlineBox() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsInlineBox();
  if (current_.item_)
    return current_.item_->IsInlineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsAtomicInline() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsAtomicInline();
  if (current_.item_)
    return current_.item_->IsAtomicInline();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEllipsis() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->IsEllipsis();
  if (current_.item_)
    return current_.item_->IsEllipsis();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedText() const {
  if (current_.paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_.paint_fragment_->PhysicalFragment()))
      return text_fragment->IsGeneratedText();
    return false;
  }
  if (current_.item_)
    return current_.item_->IsGeneratedText();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedTextType() const {
  if (current_.paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_.paint_fragment_->PhysicalFragment())) {
      return text_fragment->TextType() ==
             NGPhysicalTextFragment::kGeneratedText;
    }
    return false;
  }
  if (current_.item_)
    return current_.item_->Type() == NGFragmentItem::kGeneratedText;
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsHiddenForPaint() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsHiddenForPaint();
  if (current_.item_)
    return current_.item_->IsHiddenForPaint();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsHorizontal() const {
  return CurrentStyle().GetWritingMode() == WritingMode::kHorizontalTb;
}

bool NGInlineCursor::IsInlineLeaf() const {
  if (IsHiddenForPaint())
    return false;
  if (IsText())
    return !IsGeneratedTextType();
  if (!IsAtomicInline())
    return false;
  return !IsListMarker();
}

bool NGInlineCursor::IsPartOfCulledInlineBox(
    const LayoutInline& layout_inline) const {
  const LayoutObject* const layout_object = CurrentLayoutObject();
  // We use |IsInline()| to exclude floating and out-of-flow objects.
  if (!layout_object || !layout_object->IsInline() ||
      layout_object->IsAtomicInlineLevel())
    return false;
  DCHECK(!layout_object->IsFloatingOrOutOfFlowPositioned());
  DCHECK(!CurrentBoxFragment() ||
         !CurrentBoxFragment()->IsBlockFormattingContextRoot());
  return layout_object->IsDescendantOf(&layout_inline);
}

bool NGInlineCursor::IsLastLineInInlineBlock() const {
  DCHECK(IsLineBox());
  if (!GetLayoutBlockFlow()->IsAtomicInlineLevel())
    return false;
  NGInlineCursor next_sibling(*this);
  for (;;) {
    next_sibling.MoveToNextSibling();
    if (!next_sibling)
      return true;
    if (next_sibling.IsLineBox())
      return false;
    // There maybe other top-level objects such as floats, OOF, or list-markers.
  }
}

bool NGInlineCursor::IsLineBreak() const {
  if (current_.paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_.paint_fragment_->PhysicalFragment()))
      return text_fragment->IsLineBreak();
    return false;
  }
  if (current_.item_)
    return IsText() && current_.item_->IsLineBreak();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsListMarker() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsListMarker();
  if (current_.item_)
    return current_.item_->IsListMarker();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsText() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsText();
  if (current_.item_)
    return current_.item_->IsText();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsBeforeSoftLineBreak() const {
  if (IsLineBreak())
    return false;
  // Inline block is not be container line box.
  // See paint/selection/text-selection-inline-block.html.
  NGInlineCursor line(*this);
  line.MoveToContainingLine();
  if (line.IsLastLineInInlineBlock()) {
    // We don't paint a line break the end of inline-block
    // because if an inline-block is at the middle of line, we should not paint
    // a line break.
    // Old layout paints line break if the inline-block is at the end of line,
    // but since its complex to determine if the inline-block is at the end of
    // line on NG, we just cancels block-end line break painting for any
    // inline-block.
    return false;
  }
  NGInlineCursor last_leaf(line);
  last_leaf.MoveToLastLogicalLeaf();
  if (last_leaf != *this)
    return false;
  // Even If |fragment| is before linebreak, if its direction differs to line
  // direction, we don't paint line break. See
  // paint/selection/text-selection-newline-mixed-ltr-rtl.html.
  return line.CurrentBaseDirection() == CurrentResolvedDirection();
}

bool NGInlineCursor::CanHaveChildren() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsContainer();
  if (current_.item_) {
    return current_.item_->Type() == NGFragmentItem::kLine ||
           (current_.item_->Type() == NGFragmentItem::kBox &&
            !current_.item_->IsAtomicInline());
  }
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEmptyLineBox() const {
  DCHECK(IsLineBox());
  if (current_.paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .IsEmptyLineBox();
  }
  if (current_.item_)
    return current_.item_->IsEmptyLineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsLineBox() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().IsLineBox();
  if (current_.item_)
    return current_.item_->Type() == NGFragmentItem::kLine;
  NOTREACHED();
  return false;
}

TextDirection NGInlineCursor::CurrentBaseDirection() const {
  DCHECK(IsLineBox());
  if (current_.paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .BaseDirection();
  }
  if (current_.item_)
    return current_.item_->BaseDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

UBiDiLevel NGInlineCursor::CurrentBidiLevel() const {
  if (IsText()) {
    if (IsGeneratedTextType()) {
      // TODO(yosin): Until we have clients, we don't support bidi-level for
      // ellipsis and soft hyphens.
      NOTREACHED() << this;
      return 0;
    }
    const LayoutText& layout_text = *ToLayoutText(CurrentLayoutObject());
    DCHECK(!layout_text.NeedsLayout()) << this;
    const auto* const items = layout_text.GetNGInlineItems();
    if (!items || items->size() == 0) {
      // In case of <br>, <wbr>, text-combine-upright, etc.
      return 0;
    }
    const auto& item = std::find_if(
        items->begin(), items->end(), [this](const NGInlineItem& item) {
          return item.StartOffset() <= CurrentTextStartOffset() &&
                 item.EndOffset() >= CurrentTextEndOffset();
        });
    DCHECK(item != items->end()) << this;
    return item->BidiLevel();
  }

  if (IsAtomicInline()) {
    const NGPhysicalBoxFragment* fragmentainer =
        CurrentLayoutObject()->ContainingBlockFlowFragment();
    DCHECK(fragmentainer);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(fragmentainer->GetLayoutObject());
    const Vector<NGInlineItem> items =
        block_flow.GetNGInlineNodeData()->ItemsData(UsesFirstLineStyle()).items;
    const LayoutObject* const layout_object = CurrentLayoutObject();
    const auto* const item = std::find_if(
        items.begin(), items.end(), [layout_object](const NGInlineItem& item) {
          return item.GetLayoutObject() == layout_object;
        });
    DCHECK(item != items.end()) << this;
    return item->BidiLevel();
  }

  NOTREACHED();
  return 0;
}

const NGPhysicalBoxFragment* NGInlineCursor::CurrentBoxFragment() const {
  if (current_.paint_fragment_) {
    return DynamicTo<NGPhysicalBoxFragment>(
        &current_.paint_fragment_->PhysicalFragment());
  }
  if (current_.item_)
    return current_.item_->BoxFragment();
  NOTREACHED();
  return nullptr;
}

const DisplayItemClient* NGInlineCursor::CurrentDisplayItemClient() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_;
  if (current_.item_)
    return current_.item_;
  NOTREACHED();
  return nullptr;
}

const NGInlineBreakToken& NGInlineCursor::CurrentInlineBreakToken() const {
  DCHECK(IsLineBox());
  if (current_.paint_fragment_) {
    return To<NGInlineBreakToken>(
        *To<NGPhysicalLineBoxFragment>(
             current_.paint_fragment_->PhysicalFragment())
             .BreakToken());
  }
  DCHECK(current_.item_);
  return *current_.item_->InlineBreakToken();
}

const LayoutObject* NGInlineCursor::CurrentLayoutObject() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->GetLayoutObject();
  if (current_.item_)
    return current_.item_->GetLayoutObject();
  NOTREACHED();
  return nullptr;
}

LayoutObject* NGInlineCursor::CurrentMutableLayoutObject() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->GetMutableLayoutObject();
  if (current_.item_)
    return current_.item_->GetMutableLayoutObject();
  NOTREACHED();
  return nullptr;
}

Node* NGInlineCursor::CurrentNode() const {
  if (const LayoutObject* layout_object = CurrentLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

const PhysicalRect NGInlineCursor::CurrentInkOverflow() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->InkOverflow();
  if (current_.item_)
    return current_.item_->InkOverflow();
  NOTREACHED();
  return PhysicalRect();
}

const PhysicalOffset NGInlineCursor::CurrentOffset() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->InlineOffsetToContainerBox();
  if (current_.item_)
    return current_.item_->Offset();
  NOTREACHED();
  return PhysicalOffset();
}

const PhysicalRect NGInlineCursor::CurrentRect() const {
  return PhysicalRect(CurrentOffset(), CurrentSize());
}

TextDirection NGInlineCursor::CurrentResolvedDirection() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->PhysicalFragment().ResolvedDirection();
  if (current_.item_)
    return current_.item_->ResolvedDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

const PhysicalSize NGInlineCursor::CurrentSize() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->Size();
  if (current_.item_)
    return current_.item_->Size();
  NOTREACHED();
  return PhysicalSize();
}

const ComputedStyle& NGInlineCursor::CurrentStyle() const {
  if (current_.paint_fragment_)
    return current_.paint_fragment_->Style();
  return current_.item_->Style();
}

NGStyleVariant NGInlineCursorPosition::StyleVariant() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().StyleVariant();
  return item_->StyleVariant();
}

bool NGInlineCursor::UsesFirstLineStyle() const {
  return Current().StyleVariant() == NGStyleVariant::kFirstLine;
}

NGTextOffset NGInlineCursor::CurrentTextOffset() const {
  if (current_.paint_fragment_) {
    const auto& text_fragment = To<NGPhysicalTextFragment>(
        current_.paint_fragment_->PhysicalFragment());
    return text_fragment.TextOffset();
  }
  if (current_.item_)
    return current_.item_->TextOffset();
  NOTREACHED();
  return {};
}

StringView NGInlineCursor::CurrentText() const {
  DCHECK(IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .Text();
  }
  if (current_.item_)
    return current_.item_->Text(*fragment_items_);
  NOTREACHED();
  return "";
}

const ShapeResultView* NGInlineCursor::CurrentTextShapeResult() const {
  DCHECK(IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .TextShapeResult();
  }
  if (current_.item_)
    return current_.item_->TextShapeResult();
  NOTREACHED();
  return nullptr;
}

PhysicalRect NGInlineCursor::CurrentLocalRect(unsigned start_offset,
                                              unsigned end_offset) const {
  DCHECK(IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .LocalRect(start_offset, end_offset);
  }
  if (current_.item_) {
    return current_.item_->LocalRect(current_.item_->Text(*fragment_items_),
                                     start_offset, end_offset);
  }
  NOTREACHED();
  return PhysicalRect();
}

LayoutUnit NGInlineCursor::InlinePositionForOffset(unsigned offset) const {
  DCHECK(IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .InlinePositionForOffset(offset);
  }
  if (current_.item_) {
    return current_.item_->InlinePositionForOffset(
        current_.item_->Text(*fragment_items_), offset);
  }
  NOTREACHED();
  return LayoutUnit();
}

PhysicalOffset NGInlineCursor::LineStartPoint() const {
  DCHECK(IsLineBox()) << this;
  const LogicalOffset logical_start;  // (0, 0)
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_start.ConvertToPhysical(CurrentStyle().GetWritingMode(),
                                         CurrentBaseDirection(), CurrentSize(),
                                         pixel_size);
}

PhysicalOffset NGInlineCursor::LineEndPoint() const {
  DCHECK(IsLineBox()) << this;
  const LayoutUnit inline_size =
      IsHorizontal() ? CurrentSize().width : CurrentSize().height;
  const LogicalOffset logical_end(inline_size, LayoutUnit());
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_end.ConvertToPhysical(CurrentStyle().GetWritingMode(),
                                       CurrentBaseDirection(), CurrentSize(),
                                       pixel_size);
}

PositionWithAffinity NGInlineCursor::PositionForPointInInlineFormattingContext(
    const PhysicalOffset& point,
    const NGPhysicalBoxFragment& container) {
  DCHECK(IsItemCursor());
  const ComputedStyle& container_style = container.Style();
  const WritingMode writing_mode = container_style.GetWritingMode();
  const TextDirection direction = container_style.Direction();
  const PhysicalSize& container_size = container.Size();
  const LayoutUnit point_block_offset =
      point
          .ConvertToLogical(writing_mode, direction, container_size,
                            // |point| is actually a pixel with size 1x1.
                            PhysicalSize(LayoutUnit(1), LayoutUnit(1)))
          .block_offset;

  // Stores the closest line box child above |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_line_before;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Min();

  // Stores the closest line box child below |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_line_after;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Max();

  while (*this) {
    const NGFragmentItem* child_item = CurrentItem();
    DCHECK(child_item);
    if (child_item->Type() == NGFragmentItem::kLine) {
      // Try to resolve if |point| falls in a line box in block direction.
      const LayoutUnit child_block_offset =
          child_item->Offset()
              .ConvertToLogical(writing_mode, direction, container_size,
                                child_item->Size())
              .block_offset;
      if (point_block_offset < child_block_offset) {
        if (child_block_offset < closest_line_after_block_offset) {
          closest_line_after_block_offset = child_block_offset;
          closest_line_after = Current();
        }
        MoveToNextItemSkippingChildren();
        continue;
      }

      // Hitting on line bottom doesn't count, to match legacy behavior.
      const LayoutUnit child_block_end_offset =
          child_block_offset +
          child_item->Size().ConvertToLogical(writing_mode).block_size;
      if (point_block_offset >= child_block_end_offset) {
        if (child_block_end_offset > closest_line_before_block_offset) {
          closest_line_before_block_offset = child_block_end_offset;
          closest_line_before = Current();
        }
        MoveToNextItemSkippingChildren();
        continue;
      }

      if (const PositionWithAffinity child_position =
              PositionForPointInInlineBox(point))
        return child_position;
      MoveToNextItemSkippingChildren();
      continue;
    }
    DCHECK_NE(child_item->Type(), NGFragmentItem::kText);
    MoveToNextItem();
  }

  if (closest_line_after) {
    MoveTo(closest_line_after);
    if (const PositionWithAffinity child_position =
            PositionForPointInInlineBox(point))
      return child_position;
  }

  if (closest_line_before) {
    MoveTo(closest_line_before);
    if (const PositionWithAffinity child_position =
            PositionForPointInInlineBox(point))
      return child_position;
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForPointInInlineBox(
    const PhysicalOffset& point) const {
  if (const NGPaintFragment* paint_fragment = CurrentPaintFragment()) {
    DCHECK(paint_fragment->PhysicalFragment().IsLineBox());
    return paint_fragment->PositionForPoint(point);
  }
  const NGFragmentItem* container = CurrentItem();
  DCHECK(container);
  DCHECK_EQ(container->Type(), NGFragmentItem::kLine);
  const ComputedStyle& container_style = container->Style();
  const WritingMode writing_mode = container_style.GetWritingMode();
  const TextDirection direction = container_style.Direction();
  const PhysicalSize& container_size = container->Size();
  const LayoutUnit point_inline_offset =
      point
          .ConvertToLogical(writing_mode, direction, container_size,
                            // |point| is actually a pixel with size 1x1.
                            PhysicalSize(LayoutUnit(1), LayoutUnit(1)))
          .inline_offset;

  // Stores the closest child before |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_child_before;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_child_after;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  NGInlineCursor descendants = CursorForDescendants();
  for (; descendants; descendants.MoveToNext()) {
    const NGFragmentItem* child_item = descendants.CurrentItem();
    DCHECK(child_item);
    const LayoutUnit child_inline_offset =
        child_item->Offset()
            .ConvertToLogical(writing_mode, direction, container_size,
                              child_item->Size())
            .inline_offset;
    if (point_inline_offset < child_inline_offset) {
      if (child_inline_offset < closest_child_after_inline_offset) {
        closest_child_after_inline_offset = child_inline_offset;
        closest_child_after = descendants.Current();
      }
      continue;
    }
    const LayoutUnit child_inline_end_offset =
        child_inline_offset +
        child_item->Size().ConvertToLogical(writing_mode).inline_size;
    if (point_inline_offset > child_inline_end_offset) {
      if (child_inline_end_offset > closest_child_before_inline_offset) {
        closest_child_before_inline_offset = child_inline_end_offset;
        closest_child_before = descendants.Current();
      }
      continue;
    }

    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point, *child_item))
      return child_position;
  }

  if (closest_child_after) {
    descendants.MoveTo(closest_child_after);
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point, *closest_child_after))
      return child_position;
  }

  if (closest_child_before) {
    descendants.MoveTo(closest_child_before);
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point, *closest_child_before))
      return child_position;
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForPointInChild(
    const PhysicalOffset& point,
    const NGFragmentItem& child_item) const {
  DCHECK_EQ(&child_item, CurrentItem());
  switch (child_item.Type()) {
    case NGFragmentItem::kText:
      return child_item.PositionForPointInText(point, *this);
    case NGFragmentItem::kGeneratedText:
      break;
    case NGFragmentItem::kBox:
      if (const NGPhysicalBoxFragment* box_fragment =
              child_item.BoxFragment()) {
        // We must fallback to legacy for old layout roots. We also fallback (to
        // LayoutNGMixin::PositionForPoint()) for NG block layout, so that we
        // can utilize LayoutBlock::PositionForPoint() that resolves the
        // position in block layout.
        // TODO(xiaochengh): Don't fallback to legacy for NG block layout.
        if (box_fragment->IsBlockFlow() || box_fragment->IsLegacyLayoutRoot()) {
          return child_item.GetLayoutObject()->PositionForPoint(
              point - child_item.Offset());
        }
      }
      break;
    case NGFragmentItem::kLine:
      NOTREACHED();
      break;
  }
  return PositionWithAffinity();
}

void NGInlineCursor::MakeNull() {
  if (root_paint_fragment_) {
    current_.paint_fragment_ = nullptr;
    return;
  }
  if (fragment_items_)
    return MoveToItem(items_.end());
}

void NGInlineCursor::MoveTo(const NGInlineCursorPosition& position) {
#if DCHECK_IS_ON()
  if (position.PaintFragment()) {
    DCHECK(root_paint_fragment_);
    DCHECK(
        position.PaintFragment()->IsDescendantOfNotSelf(*root_paint_fragment_));
  } else if (position.Item()) {
    DCHECK(IsItemCursor());
    const unsigned index = position.item_iter_ - items_.begin();
    DCHECK_LT(index, items_.size());
  }
#endif
  current_ = position;
}

void NGInlineCursor::InternalMoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  // If this cursor is rootless, find the root of the inline formatting context.
  bool had_root = true;
  if (!HasRoot()) {
    had_root = false;
    const LayoutBlockFlow& root = *layout_object.RootInlineFormattingContext();
    DCHECK(&root);
    SetRoot(root);
    if (!HasRoot()) {
      const auto fragments =
          NGPaintFragment::InlineFragmentsFor(&layout_object);
      if (!fragments.IsInLayoutNGInlineFormattingContext() ||
          fragments.IsEmpty())
        return MakeNull();
      // external/wpt/css/css-scroll-anchoring/text-anchor-in-vertical-rl.html
      // reaches here.
      root_paint_fragment_ = fragments.front().Root();
    }
  }
  if (fragment_items_) {
    const wtf_size_t index = layout_object.FirstInlineFragmentItemIndex();
    if (!index) {
      // TODO(yosin): Once we update all |LayoutObject::FirstInlineFragment()|
      // clients, we should replace to |return MakeNull()|
      current_.item_iter_ = items_.begin();
      while (current_.item_ && CurrentLayoutObject() != &layout_object)
        MoveToNextItem();
      return;
    }
    DCHECK_LT(index, items_.size());
    if (!had_root)
      return MoveToItem(items_.begin() + index);
    // Map |index| in |NGFragmentItems| to index of |items_|.
    const LayoutBlockFlow& block_flow =
        *layout_object.RootInlineFormattingContext();
    const auto items =
        ItemsSpan(block_flow.CurrentFragment()->Items()->Items());
    // Note: We use address instead of iterator because we can't compare
    // iterators in different span. See |base::CheckedContiguousIterator<T>|.
    const ptrdiff_t adjusted_index =
        &*(items.begin() + index) - &*items_.begin();
    DCHECK_GE(adjusted_index, 0);
    DCHECK_LT(static_cast<size_t>(adjusted_index), items_.size());
    return MoveToItem(items_.begin() + adjusted_index);
  }
  if (root_paint_fragment_) {
    const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
    if (!fragments.IsInLayoutNGInlineFormattingContext() || fragments.IsEmpty())
      return MakeNull();
    return MoveTo(fragments.front());
  }
}

void NGInlineCursor::MoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext()) << layout_object;
  InternalMoveTo(layout_object);
  if (*this || !HasRoot()) {
    layout_inline_ = nullptr;
    return;
  }

  // This |layout_object| did not produce any fragments.
  //
  // Try to find ancestors if this is a culled inline.
  layout_inline_ = ToLayoutInlineOrNull(&layout_object);
  if (!layout_inline_)
    return;

  MoveToFirst();
  while (IsNotNull() && !IsPartOfCulledInlineBox(*layout_inline_))
    MoveToNext();
}

void NGInlineCursor::MoveTo(const NGFragmentItem& fragment_item) {
  DCHECK(!root_paint_fragment_ && !current_.paint_fragment_);
  MoveTo(*fragment_item.GetLayoutObject());
  while (IsNotNull()) {
    if (CurrentItem() == &fragment_item)
      return;
    MoveToNext();
  }
  NOTREACHED();
}

void NGInlineCursor::MoveTo(const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    MoveTo(*paint_fragment);
    return;
  }
  if (cursor.current_.item_) {
    if (!fragment_items_)
      SetRoot(*cursor.fragment_items_);
    // Note: We use address instead of iterato because we can't compare
    // iterators in different span. See |base::CheckedContiguousIterator<T>|.
    const ptrdiff_t index = &*cursor.current_.item_iter_ - &*items_.begin();
    DCHECK_GE(index, 0);
    DCHECK_LT(static_cast<size_t>(index), items_.size());
    MoveToItem(items_.begin() + index);
    return;
  }
  *this = cursor;
}

void NGInlineCursor::MoveTo(const NGPaintFragment& paint_fragment) {
  DCHECK(!fragment_items_);
  if (!root_paint_fragment_)
    root_paint_fragment_ = paint_fragment.Root();
  DCHECK(root_paint_fragment_);
  DCHECK(paint_fragment.IsDescendantOfNotSelf(*root_paint_fragment_))
      << paint_fragment << " " << root_paint_fragment_;
  current_.paint_fragment_ = &paint_fragment;
}

void NGInlineCursor::MoveToContainingLine() {
  DCHECK(!IsLineBox());
  if (current_.paint_fragment_) {
    current_.paint_fragment_ = current_.paint_fragment_->ContainerLineBox();
    return;
  }
  if (current_.item_) {
    while (current_.item_ && !IsLineBox())
      MoveToPreviousItem();
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirst() {
  if (root_paint_fragment_) {
    current_.paint_fragment_ = root_paint_fragment_->FirstChild();
    return;
  }
  if (IsItemCursor()) {
    MoveToItem(items_.begin());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirstChild() {
  DCHECK(CanHaveChildren());
  if (!TryToMoveToFirstChild())
    MakeNull();
}

void NGInlineCursor::MoveToFirstLogicalLeaf() {
  DCHECK(IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(CurrentStyle().Direction())) {
    while (TryToMoveToFirstChild())
      continue;
    return;
  }
  while (TryToMoveToLastChild())
    continue;
}

void NGInlineCursor::MoveToLastChild() {
  DCHECK(CanHaveChildren());
  if (!TryToMoveToLastChild())
    MakeNull();
}

void NGInlineCursor::MoveToLastForSameLayoutObject() {
  NGInlineCursor last;
  while (IsNotNull()) {
    last = *this;
    MoveToNextForSameLayoutObject();
  }
  *this = last;
}

void NGInlineCursor::MoveToLastLogicalLeaf() {
  DCHECK(IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(CurrentStyle().Direction())) {
    while (TryToMoveToLastChild())
      continue;
    return;
  }
  while (TryToMoveToFirstChild())
    continue;
}

void NGInlineCursor::MoveToNext() {
  if (root_paint_fragment_)
    return MoveToNextPaintFragment();
  MoveToNextItem();
}

void NGInlineCursor::MoveToNextForSameLayoutObject() {
  if (layout_inline_) {
    // Move to next fragment in culled inline box undef |layout_inline_|.
    do {
      MoveToNext();
    } while (IsNotNull() && !IsPartOfCulledInlineBox(*layout_inline_));
    return;
  }
  if (current_.paint_fragment_) {
    if (auto* paint_fragment =
            current_.paint_fragment_->NextForSameLayoutObject()) {
      // |paint_fragment| can be in another fragment tree rooted by
      // |root_paint_fragment_|, e.g. "multicol-span-all-restyle-002.html"
      root_paint_fragment_ = paint_fragment->Root();
      return MoveTo(*paint_fragment);
    }
    return MakeNull();
  }
  if (current_.item_) {
    const wtf_size_t delta = current_.item_->DeltaToNextForSameLayoutObject();
    if (delta == 0u)
      return MakeNull();
    return MoveToItem(current_.item_iter_ + delta);
  }
}

void NGInlineCursor::MoveToNextInlineLeaf() {
  if (IsNotNull() && IsInlineLeaf())
    MoveToNext();
  while (IsNotNull() && !IsInlineLeaf())
    MoveToNext();
}

void NGInlineCursor::MoveToNextInlineLeafIgnoringLineBreak() {
  do {
    MoveToNextInlineLeaf();
  } while (IsNotNull() && IsLineBreak());
}

void NGInlineCursor::MoveToNextInlineLeafOnLine() {
  MoveToLastForSameLayoutObject();
  if (IsNull())
    return;
  NGInlineCursor last_item = *this;
  MoveToContainingLine();
  NGInlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(last_item);
  // Note: AX requires this for AccessibilityLayoutTest.NextOnLine.
  if (!cursor.IsInlineLeaf())
    cursor.MoveToNextInlineLeaf();
  cursor.MoveToNextInlineLeaf();
  MoveTo(cursor);
}

void NGInlineCursor::MoveToNextLine() {
  DCHECK(IsLineBox());
  if (current_.paint_fragment_) {
    if (auto* paint_fragment = current_.paint_fragment_->NextSibling())
      return MoveTo(*paint_fragment);
    return MakeNull();
  }
  if (current_.item_) {
    do {
      MoveToNextItem();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToNextSibling() {
  if (current_.paint_fragment_)
    return MoveToNextSiblingPaintFragment();
  return MoveToNextSiblingItem();
}

void NGInlineCursor::MoveToNextSkippingChildren() {
  if (root_paint_fragment_)
    return MoveToNextPaintFragmentSkippingChildren();
  MoveToNextItemSkippingChildren();
}

void NGInlineCursor::MoveToPrevious() {
  if (root_paint_fragment_)
    return MoveToPreviousPaintFragment();
  MoveToPreviousItem();
}

void NGInlineCursor::MoveToPreviousInlineLeaf() {
  if (IsNotNull() && IsInlineLeaf())
    MoveToPrevious();
  while (IsNotNull() && !IsInlineLeaf())
    MoveToPrevious();
}

void NGInlineCursor::MoveToPreviousInlineLeafIgnoringLineBreak() {
  do {
    MoveToPreviousInlineLeaf();
  } while (IsNotNull() && IsLineBreak());
}

void NGInlineCursor::MoveToPreviousInlineLeafOnLine() {
  if (IsNull())
    return;
  NGInlineCursor first_item = *this;
  MoveToContainingLine();
  NGInlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(first_item);
  // Note: AX requires this for AccessibilityLayoutTest.NextOnLine.
  if (!cursor.IsInlineLeaf())
    cursor.MoveToPreviousInlineLeaf();
  cursor.MoveToPreviousInlineLeaf();
  MoveTo(cursor);
}

void NGInlineCursor::MoveToPreviousLine() {
  // Note: List marker is sibling of line box.
  DCHECK(IsLineBox());
  if (current_.paint_fragment_) {
    do {
      MoveToPreviousSiblingPaintFragment();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  if (current_.item_) {
    do {
      MoveToPreviousItem();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::TryToMoveToFirstChild() {
  if (!HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(*current_.paint_fragment_->FirstChild());
    return true;
  }
  MoveToItem(current_.item_iter_ + 1);
  return true;
}

bool NGInlineCursor::TryToMoveToLastChild() {
  if (!HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(current_.paint_fragment_->Children().back());
    return true;
  }
  const auto end = current_.item_iter_ + CurrentItem()->DescendantsCount();
  MoveToNextItem();
  DCHECK(!IsNull());
  for (auto it = current_.item_iter_ + 1; it != end; ++it) {
    if (CurrentItem()->HasSameParent(**it))
      MoveToItem(it);
  }
  return true;
}

void NGInlineCursor::MoveToNextItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  DCHECK(current_.item_iter_ != items_.end());
  ++current_.item_iter_;
  MoveToItem(current_.item_iter_);
}

void NGInlineCursor::MoveToNextItemSkippingChildren() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_.item_->DescendantsCount())
    return MoveToItem(current_.item_iter_ + descendants_count);
  return MoveToNextItem();
}

void NGInlineCursor::MoveToNextSiblingItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  const NGFragmentItem& item = *CurrentItem();
  MoveToNextItemSkippingChildren();
  if (IsNull() || item.HasSameParent(*CurrentItem()))
    return;
  MakeNull();
}

void NGInlineCursor::MoveToPreviousItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  if (current_.item_iter_ == items_.begin())
    return MakeNull();
  --current_.item_iter_;
  current_.item_ = current_.item_iter_->get();
}

void NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* parent = current_.paint_fragment_->Parent();
  if (parent && parent != root_paint_fragment_) {
    current_.paint_fragment_ = parent;
    return;
  }
  current_.paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  if (const NGPaintFragment* child = current_.paint_fragment_->FirstChild()) {
    current_.paint_fragment_ = child;
    return;
  }
  MoveToNextPaintFragmentSkippingChildren();
}

void NGInlineCursor::MoveToNextSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  if (const NGPaintFragment* next = current_.paint_fragment_->NextSibling()) {
    current_.paint_fragment_ = next;
    return;
  }
  current_.paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  while (current_.paint_fragment_) {
    if (const NGPaintFragment* next = current_.paint_fragment_->NextSibling()) {
      current_.paint_fragment_ = next;
      return;
    }
    MoveToParentPaintFragment();
  }
}

void NGInlineCursor::MoveToPreviousPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* const parent = current_.paint_fragment_->Parent();
  MoveToPreviousSiblingPaintFragment();
  if (current_.paint_fragment_) {
    while (TryToMoveToLastChild())
      continue;
    return;
  }
  current_.paint_fragment_ = parent == root_paint_fragment_ ? nullptr : parent;
}

void NGInlineCursor::MoveToPreviousSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* const current = current_.paint_fragment_;
  current_.paint_fragment_ = nullptr;
  for (auto* sibling : current->Parent()->Children()) {
    if (sibling == current)
      return;
    current_.paint_fragment_ = sibling;
  }
  NOTREACHED();
}

NGInlineBackwardCursor::NGInlineBackwardCursor(const NGInlineCursor& cursor)
    : cursor_(cursor) {
  if (cursor.root_paint_fragment_) {
    for (NGInlineCursor sibling(cursor); sibling; sibling.MoveToNextSibling())
      sibling_paint_fragments_.push_back(sibling.CurrentPaintFragment());
    current_index_ = sibling_paint_fragments_.size();
    if (current_index_)
      current_paint_fragment_ = sibling_paint_fragments_[--current_index_];
    return;
  }
  if (cursor.IsItemCursor()) {
    for (NGInlineCursor sibling(cursor); sibling; sibling.MoveToNextSibling())
      sibling_item_iterators_.push_back(sibling.Current().item_iter_);
    current_index_ = sibling_item_iterators_.size();
    if (current_index_)
      current_item_ = sibling_item_iterators_[--current_index_]->get();
    return;
  }
  NOTREACHED();
}

NGInlineCursor NGInlineBackwardCursor::CursorForDescendants() const {
  if (const NGPaintFragment* current_paint_fragment = CurrentPaintFragment())
    return NGInlineCursor(*current_paint_fragment);
  if (current_item_) {
    NGInlineCursor cursor(cursor_);
    cursor.MoveToItem(sibling_item_iterators_[current_index_]);
    return cursor.CursorForDescendants();
  }
  NOTREACHED();
  return NGInlineCursor();
}

const PhysicalOffset NGInlineBackwardCursor::CurrentOffset() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->InlineOffsetToContainerBox();
  if (current_item_)
    return current_item_->Offset();
  NOTREACHED();
  return PhysicalOffset();
}

const PhysicalRect NGInlineBackwardCursor::CurrentSelfInkOverflow() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->SelfInkOverflow();
  if (current_item_)
    return current_item_->SelfInkOverflow();
  NOTREACHED();
  return PhysicalRect();
}

void NGInlineBackwardCursor::MoveToPreviousSibling() {
  if (current_index_) {
    if (current_paint_fragment_) {
      current_paint_fragment_ = sibling_paint_fragments_[--current_index_];
      return;
    }
    if (current_item_) {
      current_item_ = sibling_item_iterators_[--current_index_]->get();
      return;
    }
    NOTREACHED();
  }
  current_paint_fragment_ = nullptr;
  current_item_ = nullptr;
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor& cursor) {
  if (cursor.IsNull())
    return ostream << "NGInlineCursor()";
  if (cursor.IsPaintFragmentCursor()) {
    return ostream << "NGInlineCursor(" << *cursor.CurrentPaintFragment()
                   << ")";
  }
  DCHECK(cursor.IsItemCursor());
  return ostream << "NGInlineCursor(" << *cursor.CurrentItem() << ")";
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor* cursor) {
  if (!cursor)
    return ostream << "<null>";
  return ostream << *cursor;
}

}  // namespace blink
