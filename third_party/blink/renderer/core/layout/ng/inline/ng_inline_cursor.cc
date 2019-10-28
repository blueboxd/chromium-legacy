// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(IsItemCursor());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  item_iter_ = iter;
  current_item_ = iter == items_.end() ? nullptr : iter->get();
}

void NGInlineCursor::SetRoot(ItemsSpan items) {
  DCHECK(items.data() || !items.size());
  DCHECK_EQ(root_paint_fragment_, nullptr);
  DCHECK_EQ(current_paint_fragment_, nullptr);
  items_ = items;
  MoveToItem(items_.begin());
}

void NGInlineCursor::SetRoot(const NGFragmentItems& items) {
  SetRoot(items.Items());
}

void NGInlineCursor::SetRoot(const NGPaintFragment& root_paint_fragment) {
  DCHECK(&root_paint_fragment);
  DCHECK(!root_paint_fragment.Parent()) << root_paint_fragment;
  root_paint_fragment_ = &root_paint_fragment;
  current_paint_fragment_ = root_paint_fragment.FirstChild();
}

NGInlineCursor::NGInlineCursor(const LayoutBlockFlow& block_flow) {
  DCHECK(&block_flow);

  if (const NGPhysicalBoxFragment* fragment = block_flow.CurrentFragment()) {
    if (const NGFragmentItems* items = fragment->Items()) {
      fragment_items_ = items;
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

NGInlineCursor::NGInlineCursor(const NGFragmentItems& items)
    : fragment_items_(&items) {
  SetRoot(items);
}

NGInlineCursor::NGInlineCursor(const NGPaintFragment& root_paint_fragment) {
  SetRoot(root_paint_fragment);
}

NGInlineCursor::NGInlineCursor(const NGInlineCursor& other)
    : items_(other.items_),
      item_iter_(other.item_iter_),
      current_item_(other.current_item_),
      fragment_items_(other.fragment_items_),
      root_paint_fragment_(other.root_paint_fragment_),
      current_paint_fragment_(other.current_paint_fragment_),
      layout_inline_(other.layout_inline_) {}

NGInlineCursor::NGInlineCursor() = default;

bool NGInlineCursor::operator==(const NGInlineCursor& other) const {
  if (root_paint_fragment_) {
    return root_paint_fragment_ == other.root_paint_fragment_ &&
           current_paint_fragment_ == other.current_paint_fragment_;
  }
  if (current_item_ != other.current_item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(item_iter_ == other.item_iter_);
  return true;
}

const LayoutBlockFlow* NGInlineCursor::GetLayoutBlockFlow() const {
  if (root_paint_fragment_)
    return To<LayoutBlockFlow>(root_paint_fragment_->GetLayoutObject());
  for (const auto& item : items_) {
    if (item->GetLayoutObject() && item->GetLayoutObject()->IsInline())
      return item->GetLayoutObject()->RootInlineFormattingContext();
  }
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursor::HasChildren() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->FirstChild();
  if (current_item_) {
    // Note: |DescendantsCount() == 1| means box/line without children.
    return current_item_->DescendantsCount() > 1;
  }
  NOTREACHED();
  return false;
}

bool NGInlineCursor::HasSoftWrapToNextLine() const {
  DCHECK(IsLineBox());
  const NGInlineBreakToken& break_token = CurrentInlineBreakToken();
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

bool NGInlineCursor::IsAtomicInline() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsAtomicInline();
  if (current_item_)
    return current_item_->IsAtomicInline();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEllipsis() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->IsEllipsis();
  if (current_item_)
    return current_item_->IsEllipsis();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedText() const {
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment()))
      return text_fragment->IsGeneratedText();
    return false;
  }
  if (current_item_)
    return current_item_->IsGeneratedText();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedTextType() const {
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment())) {
      return text_fragment->TextType() ==
             NGPhysicalTextFragment::kGeneratedText;
    }
    return false;
  }
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kGeneratedText;
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsHiddenForPaint() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsHiddenForPaint();
  if (current_item_)
    return current_item_->IsHiddenForPaint();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsInlineLeaf() const {
  if (IsText())
    return true;
  if (!IsAtomicInline())
    return false;
  return !IsListMarker();
}

bool NGInlineCursor::IsInclusiveDescendantOf(
    const LayoutObject& layout_object) const {
  return CurrentLayoutObject() &&
         CurrentLayoutObject()->IsDescendantOf(&layout_object);
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
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment()))
      return text_fragment->IsLineBreak();
    return false;
  }
  if (current_item_)
    return current_item_->IsLineBreak();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsListMarker() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsListMarker();
  if (current_item_)
    return current_item_->IsListMarker();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsText() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsText();
  if (current_item_) {
    return current_item_->Type() == NGFragmentItem::kText ||
           current_item_->Type() == NGFragmentItem::kGeneratedText;
  }
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
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsContainer();
  if (current_item_) {
    return current_item_->Type() == NGFragmentItem::kLine ||
           (current_item_->Type() == NGFragmentItem::kBox &&
            !current_item_->IsAtomicInline());
  }
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEmptyLineBox() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_paint_fragment_->PhysicalFragment())
        .IsEmptyLineBox();
  }
  if (current_item_)
    return current_item_->IsEmptyLineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsLineBox() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsLineBox();
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kLine;
  NOTREACHED();
  return false;
}

TextDirection NGInlineCursor::CurrentBaseDirection() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_paint_fragment_->PhysicalFragment())
        .BaseDirection();
  }
  if (current_item_)
    return current_item_->BaseDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

const NGPhysicalBoxFragment* NGInlineCursor::CurrentBoxFragment() const {
  if (current_paint_fragment_) {
    return DynamicTo<NGPhysicalBoxFragment>(
        &current_paint_fragment_->PhysicalFragment());
  }
  if (current_item_)
    return current_item_->BoxFragment();
  NOTREACHED();
  return nullptr;
}

const NGInlineBreakToken& NGInlineCursor::CurrentInlineBreakToken() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGInlineBreakToken>(
        *To<NGPhysicalLineBoxFragment>(
             current_paint_fragment_->PhysicalFragment())
             .BreakToken());
  }
  DCHECK(current_item_);
  return *current_item_->InlineBreakToken();
}

const LayoutObject* NGInlineCursor::CurrentLayoutObject() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->GetLayoutObject();
  if (current_item_)
    return current_item_->GetLayoutObject();
  NOTREACHED();
  return nullptr;
}

Node* NGInlineCursor::CurrentNode() const {
  if (const LayoutObject* layout_object = CurrentLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

const PhysicalOffset NGInlineCursor::CurrentOffset() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->InlineOffsetToContainerBox();
  if (current_item_)
    return current_item_->Offset();
  NOTREACHED();
  return PhysicalOffset();
}

const PhysicalRect NGInlineCursor::CurrentRect() const {
  return PhysicalRect(CurrentOffset(), CurrentSize());
}

TextDirection NGInlineCursor::CurrentResolvedDirection() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().ResolvedDirection();
  if (current_item_)
    return current_item_->ResolvedDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

const PhysicalSize NGInlineCursor::CurrentSize() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->Size();
  if (current_item_)
    return current_item_->Size();
  NOTREACHED();
  return PhysicalSize();
}

const ComputedStyle& NGInlineCursor::CurrentStyle() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->Style();
  return current_item_->Style();
}

unsigned NGInlineCursor::CurrentTextStartOffset() const {
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .StartOffset();
  }
  if (current_item_)
    return current_item_->StartOffset();
  NOTREACHED();
  return 0u;
}

unsigned NGInlineCursor::CurrentTextEndOffset() const {
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .EndOffset();
  }
  if (current_item_)
    return current_item_->EndOffset();
  NOTREACHED();
  return 0u;
}

PhysicalRect NGInlineCursor::CurrentLocalRect(unsigned start_offset,
                                              unsigned end_offset) const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .LocalRect(start_offset, end_offset);
  }
  if (current_item_) {
    return current_item_->LocalRect(current_item_->Text(*fragment_items_),
                                    start_offset, end_offset);
  }
  NOTREACHED();
  return PhysicalRect();
}

LayoutUnit NGInlineCursor::InlinePositionForOffset(unsigned offset) const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .InlinePositionForOffset(offset);
  }
  if (current_item_) {
    return current_item_->InlinePositionForOffset(
        current_item_->Text(*fragment_items_), offset);
  }
  NOTREACHED();
  return LayoutUnit();
}

void NGInlineCursor::MakeNull() {
  if (root_paint_fragment_) {
    current_paint_fragment_ = nullptr;
    return;
  }
  if (fragment_items_)
    return MoveToItem(items_.end());
}

void NGInlineCursor::InternalMoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  if (fragment_items_) {
    item_iter_ = items_.begin();
    while (current_item_ && CurrentLayoutObject() != &layout_object)
      MoveToNextItem();
    return;
  }
  const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
  if (!fragments.IsInLayoutNGInlineFormattingContext() || fragments.IsEmpty())
    return MakeNull();
  if (!root_paint_fragment_) {
    // We reach here in case of |ScrollANchor::NotifyBeforeLayout()| via
    // |LayoutText::PhysicalLinesBoundingBox()|
    // See external/wpt/css/css-scroll-anchoring/wrapped-text.html
    root_paint_fragment_ = fragments.front().Root();
  }
  return MoveTo(fragments.front());
}

void NGInlineCursor::MoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext()) << layout_object;
  InternalMoveTo(layout_object);
  if (IsNotNull() || !layout_object.IsLayoutInline()) {
    layout_inline_ = nullptr;
    return;
  }
  // In case of |layout_object| is cullred inline.
  if (!fragment_items_ && !root_paint_fragment_) {
    root_paint_fragment_ =
        layout_object.RootInlineFormattingContext()->PaintFragment();
    if (!root_paint_fragment_)
      return MakeNull();
  }
  layout_inline_ = ToLayoutInline(&layout_object);
  MoveToFirst();
  while (IsNotNull() && !IsInclusiveDescendantOf(layout_object))
    MoveToNext();
}

void NGInlineCursor::MoveTo(const NGPaintFragment& paint_fragment) {
  DCHECK(!fragment_items_);
  if (!root_paint_fragment_)
    root_paint_fragment_ = paint_fragment.Root();
  DCHECK(root_paint_fragment_);
  DCHECK(paint_fragment.IsDescendantOfNotSelf(*root_paint_fragment_))
      << paint_fragment << " " << root_paint_fragment_;
  current_paint_fragment_ = &paint_fragment;
}

void NGInlineCursor::MoveToContainingLine() {
  DCHECK(!IsLineBox());
  if (current_paint_fragment_) {
    current_paint_fragment_ = current_paint_fragment_->ContainerLineBox();
    return;
  }
  if (current_item_) {
    do {
      MoveToPreviousItem();
    } while (current_item_ && !IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirst() {
  if (root_paint_fragment_) {
    current_paint_fragment_ = root_paint_fragment_->FirstChild();
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
    } while (IsNotNull() && !IsInclusiveDescendantOf(*layout_inline_));
    return;
  }
  if (current_paint_fragment_) {
    if (auto* paint_fragment =
            current_paint_fragment_->NextForSameLayoutObject()) {
      // |paint_fragment| can be in another fragment tree rooted by
      // |root_paint_fragment_|, e.g. "multicol-span-all-restyle-002.html"
      root_paint_fragment_ = paint_fragment->Root();
      return MoveTo(*paint_fragment);
    }
    return MakeNull();
  }
  if (current_item_) {
    const LayoutObject* const layout_object = CurrentLayoutObject();
    DCHECK(layout_object);
    do {
      MoveToNextItem();
    } while (current_item_ && CurrentLayoutObject() != layout_object);
    return;
  }
}

void NGInlineCursor::MoveToNextLine() {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    if (auto* paint_fragment = current_paint_fragment_->NextSibling())
      return MoveTo(*paint_fragment);
    return MakeNull();
  }
  if (current_item_) {
    do {
      MoveToNextItem();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToNextSibling() {
  if (current_paint_fragment_)
    return MoveToNextSiblingPaintFragment();
  return MoveToNextSiblingItem();
}

void NGInlineCursor::MoveToNextSkippingChildren() {
  if (root_paint_fragment_)
    return MoveToNextPaintFragmentSkippingChildren();
  MoveToNextItemSkippingChildren();
}

void NGInlineCursor::MoveToPreviousLine() {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    // TODO(yosin): We should implement |PreviousLineOf()| here.
    if (auto* paint_fragment =
            NGPaintFragmentTraversal::PreviousLineOf(*current_paint_fragment_))
      return MoveTo(*paint_fragment);
    return MakeNull();
  }
  if (current_item_) {
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
    MoveTo(*current_paint_fragment_->FirstChild());
    return true;
  }
  MoveToItem(item_iter_ + 1);
  return true;
}

bool NGInlineCursor::TryToMoveToLastChild() {
  if (!HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(current_paint_fragment_->Children().back());
    return true;
  }
  const auto end = item_iter_ + CurrentItem()->DescendantsCount();
  MoveToNextItem();
  DCHECK(!IsNull());
  for (auto it = item_iter_ + 1; it != end; ++it) {
    if (CurrentItem()->HasSameParent(**it))
      MoveToItem(it);
  }
  return true;
}

void NGInlineCursor::MoveToNextItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  DCHECK(item_iter_ != items_.end());
  ++item_iter_;
  MoveToItem(item_iter_);
}

void NGInlineCursor::MoveToNextItemSkippingChildren() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_item_->DescendantsCount())
    return MoveToItem(item_iter_ + descendants_count);
  return MoveToNextItem();
}

void NGInlineCursor::MoveToNextSiblingItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  const NGFragmentItem& item = *CurrentItem();
  MoveToNextItemSkippingChildren();
  if (IsNull() || item.HasSameParent(*CurrentItem()))
    return;
  MakeNull();
}

void NGInlineCursor::MoveToPreviousItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  if (item_iter_ == items_.begin())
    return MakeNull();
  --item_iter_;
  current_item_ = item_iter_->get();
}

void NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  const NGPaintFragment* parent = current_paint_fragment_->Parent();
  if (parent && parent != root_paint_fragment_) {
    current_paint_fragment_ = parent;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  if (const NGPaintFragment* child = current_paint_fragment_->FirstChild()) {
    current_paint_fragment_ = child;
    return;
  }
  MoveToNextPaintFragmentSkippingChildren();
}

void NGInlineCursor::MoveToNextSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
    current_paint_fragment_ = next;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  while (current_paint_fragment_) {
    if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
      current_paint_fragment_ = next;
      return;
    }
    MoveToParentPaintFragment();
  }
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
