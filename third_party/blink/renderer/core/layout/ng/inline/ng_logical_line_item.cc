// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"

namespace blink {

void NGLogicalLineItems::CreateTextFragments(WritingMode writing_mode,
                                             const String& text_content) {
  NGTextFragmentBuilder text_builder(writing_mode);
  for (auto& child : *this) {
    if (NGInlineItemResult* item_result = child.item_result) {
      DCHECK(item_result->item);
      const NGInlineItem& item = *item_result->item;
      DCHECK(item.Type() == NGInlineItem::kText ||
             item.Type() == NGInlineItem::kControl);
      DCHECK(item.TextType() == NGTextType::kNormal ||
             item.TextType() == NGTextType::kSymbolMarker);
      text_builder.SetItem(text_content, item_result,
                           child.rect.size.block_size);
      DCHECK(!child.fragment);
      child.fragment = text_builder.ToTextFragment();
    }
  }
}

NGLogicalLineItem* NGLogicalLineItems::FirstInFlowChild() {
  for (auto& child : *this) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

NGLogicalLineItem* NGLogicalLineItems::LastInFlowChild() {
  for (auto it = rbegin(); it != rend(); it++) {
    auto& child = *it;
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

void NGLogicalLineItems::WillInsertChild(unsigned insert_before) {
  unsigned index = 0;
  for (NGLogicalLineItem& child : children_) {
    if (index >= insert_before)
      break;
    if (child.children_count && index + child.children_count > insert_before)
      ++child.children_count;
    ++index;
  }
}

void NGLogicalLineItems::InsertChild(unsigned index) {
  WillInsertChild(index);
  children_.insert(index, NGLogicalLineItem());
}

void NGLogicalLineItems::MoveInInlineDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.inline_offset += delta;
}

void NGLogicalLineItems::MoveInInlineDirection(LayoutUnit delta,
                                               unsigned start,
                                               unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.inline_offset += delta;
}

void NGLogicalLineItems::MoveInBlockDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.block_offset += delta;
}

void NGLogicalLineItems::MoveInBlockDirection(LayoutUnit delta,
                                              unsigned start,
                                              unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.block_offset += delta;
}

}  // namespace blink
