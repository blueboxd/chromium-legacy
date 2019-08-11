// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

NGInlineCursor::NGInlineCursor(LayoutBlockFlow* block_flow) {
  DCHECK(block_flow);

  if (const NGPhysicalBoxFragment* fragment = block_flow->CurrentFragment()) {
    if (const NGFragmentItems* items = fragment->Items()) {
      SetRoot(items);
      return;
    }
  }

  if (const NGPaintFragment* paint_fragment = block_flow->PaintFragment()) {
    SetRoot(paint_fragment);
    return;
  }

  NOTREACHED();
}

void NGInlineCursor::SetRoot(const NGFragmentItems* items) {
  items_ = items;
}

void NGInlineCursor::SetRoot(const NGPaintFragment* root_paint_fragment) {
  root_paint_fragment_ = root_paint_fragment;
  current_paint_fragment_ = root_paint_fragment;
}

bool NGInlineCursor::IsLineBox() const {
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kLine;
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsLineBox();
  NOTREACHED();
  return false;
}

const LayoutObject* NGInlineCursor::CurrentLayoutObject() const {
  if (current_item_)
    return current_item_->GetLayoutObject();
  if (current_paint_fragment_)
    return current_paint_fragment_->GetLayoutObject();
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursor::MoveToNext() {
  if (items_)
    return MoveToNextItem();
  if (root_paint_fragment_)
    return MoveToNextPaintFragment();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::MoveToNextItem() {
  DCHECK(items_);
  if (next_item_index_ < items_->Items().size()) {
    current_item_ = items_->Items()[next_item_index_++].get();
    return true;
  }
  return false;
}

bool NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  const NGPaintFragment* parent = current_paint_fragment_->Parent();
  DCHECK(parent);
  if (parent == root_paint_fragment_)
    return false;
  current_paint_fragment_ = parent;
  return true;
}

bool NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  if (const NGPaintFragment* child = current_paint_fragment_->FirstChild()) {
    current_paint_fragment_ = child;
    return true;
  }
  return MoveToNextPaintFragmentSkippingChildren();
}

bool NGInlineCursor::MoveToNextSibilingPaintFragment() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
    current_paint_fragment_ = next;
    return true;
  }
  return false;
}

bool NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(root_paint_fragment_ && current_paint_fragment_);
  while (true) {
    if (MoveToNextSibilingPaintFragment())
      return true;
    if (!MoveToParentPaintFragment())
      return false;
  }
}

}  // namespace blink
