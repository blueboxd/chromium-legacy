// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK((items_.data() || !items_.size()) && !root_paint_fragment_);
  DCHECK(iter == items_.end() ||
         (&*iter >= items_.data() && iter < items_.end()));
  item_iter_ = iter;
  current_item_ = item_iter_ != items_.end() ? item_iter_->get() : nullptr;
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
  root_paint_fragment_ = &root_paint_fragment;
  current_paint_fragment_ = root_paint_fragment.FirstChild();
}

NGInlineCursor::NGInlineCursor(const LayoutBlockFlow& block_flow) {
  DCHECK(&block_flow);

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

  NOTREACHED();
}

NGInlineCursor::NGInlineCursor(const NGFragmentItems& items)
    : fragment_items_(&items) {
  SetRoot(items);
}

NGInlineCursor::NGInlineCursor(const NGPaintFragment& root_paint_fragment) {
  SetRoot(root_paint_fragment);
}

bool NGInlineCursor::IsLineBox() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsLineBox();
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kLine;
  NOTREACHED();
  return false;
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

const LayoutObject* NGInlineCursor::CurrentLayoutObject() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->GetLayoutObject();
  if (current_item_)
    return current_item_->GetLayoutObject();
  NOTREACHED();
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

void NGInlineCursor::MoveToNext() {
  if (root_paint_fragment_) {
    MoveToNextPaintFragment();
    return;
  }
  MoveToNextItem();
}

void NGInlineCursor::MoveToNextSkippingChildren() {
  if (root_paint_fragment_) {
    MoveToNextPaintFragmentSkippingChildren();
    return;
  }
  MoveToNextItemSkippingChildren();
}

void NGInlineCursor::MoveToNextItem() {
  DCHECK((items_.data() || !items_.size()) && !root_paint_fragment_);
  if (current_item_) {
    DCHECK(item_iter_ != items_.end());
    ++item_iter_;
    current_item_ = item_iter_ != items_.end() ? item_iter_->get() : nullptr;
  }
}

void NGInlineCursor::MoveToNextItemSkippingChildren() {
  DCHECK((items_.data() || !items_.size()) && !root_paint_fragment_);
  if (UNLIKELY(!current_item_))
    return;
  // If the current item has |ChildrenCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t children_count = current_item_->ChildrenCount()) {
    MoveToItem(item_iter_ + children_count);
    return;
  }
  return MoveToNextItem();
}

void NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  const NGPaintFragment* parent = current_paint_fragment_->Parent();
  if (parent && parent != root_paint_fragment_) {
    current_paint_fragment_ = parent;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  if (const NGPaintFragment* child = current_paint_fragment_->FirstChild()) {
    current_paint_fragment_ = child;
    return;
  }
  MoveToNextPaintFragmentSkippingChildren();
}

void NGInlineCursor::MoveToNextSibilingPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
    current_paint_fragment_ = next;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  while (!IsAtEnd()) {
    if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
      current_paint_fragment_ = next;
      return;
    }
    MoveToParentPaintFragment();
  }
}

}  // namespace blink
