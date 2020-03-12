// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_child_iterator.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NGFragmentChildIterator::NGFragmentChildIterator(
    const NGPhysicalBoxFragment& parent)
    : parent_fragment_(&parent) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  current_.link_.fragment = nullptr;
  if (parent.HasItems()) {
    current_.cursor_.emplace(*parent.Items());
    UpdateSelfFromCursor();
  } else {
    UpdateSelfFromFragment();
  }
}

NGFragmentChildIterator::NGFragmentChildIterator(const NGInlineCursor& parent) {
  current_.link_.fragment = nullptr;
  current_.cursor_ = parent.CursorForDescendants();
  UpdateSelfFromCursor();
}

NGFragmentChildIterator NGFragmentChildIterator::Descend() const {
  if (current_.cursor_) {
    const NGFragmentItem* item = current_.cursor_->CurrentItem();
    // Descend using the cursor if the current item doesn't establish a new
    // formatting context.
    if (!item->IsBlockFormattingContextRoot())
      return NGFragmentChildIterator(*current_.cursor_);
  }
  DCHECK(current_.BoxFragment());
  return NGFragmentChildIterator(*current_.BoxFragment());
}

bool NGFragmentChildIterator::AdvanceChildFragment() {
  DCHECK(parent_fragment_);
  const auto children = parent_fragment_->Children();
  if (child_fragment_idx_ < children.size())
    child_fragment_idx_++;
  // There may be line box fragments among the children, and we're not
  // interested in them (lines will already have been handled by the inline
  // cursor).
  SkipToBoxFragment();
  if (child_fragment_idx_ >= children.size())
    return false;
  UpdateSelfFromFragment();
  return true;
}

void NGFragmentChildIterator::UpdateSelfFromFragment() {
  DCHECK(parent_fragment_);
  const auto children = parent_fragment_->Children();
  if (child_fragment_idx_ >= children.size())
    return;
  current_.link_ = children[child_fragment_idx_];
  DCHECK(current_.link_.fragment);
}

bool NGFragmentChildIterator::AdvanceWithCursor() {
  DCHECK(current_.cursor_);
  current_.cursor_->MoveToNextSkippingChildren();
  UpdateSelfFromCursor();
  if (current_.cursor_->CurrentItem())
    return true;
  // If there are more items, proceed and see if we have box fragment
  // children. There may be out-of-flow positioned child fragments.
  if (!parent_fragment_)
    return false;
  current_.cursor_.reset();
  SkipToBoxFragment();
  UpdateSelfFromFragment();
  return !IsAtEnd();
}

void NGFragmentChildIterator::UpdateSelfFromCursor() {
  DCHECK(current_.cursor_);
  const NGFragmentItem* item = current_.cursor_->CurrentItem();
  if (!item) {
    current_.link_.fragment = nullptr;
    return;
  }
  current_.link_ = {item->BoxFragment(), item->OffsetInContainerBlock()};
}

void NGFragmentChildIterator::SkipToBoxFragment() {
  for (const auto children = parent_fragment_->Children();
       child_fragment_idx_ < children.size(); child_fragment_idx_++) {
    if (children[child_fragment_idx_].fragment->IsBox())
      break;
  }
}

}  // namespace blink
