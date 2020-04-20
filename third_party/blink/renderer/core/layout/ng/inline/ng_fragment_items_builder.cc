// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGFragmentItemsBuilder::NGFragmentItemsBuilder(const NGInlineNode& node) {
  const NGInlineItemsData& items_data = node.ItemsData(false);
  text_content_ = items_data.text_content;
  const NGInlineItemsData& first_line = node.ItemsData(true);
  if (&items_data != &first_line)
    first_line_text_content_ = first_line.text_content;

  // For a very large inline formatting context, the vector reallocation becomes
  // hot. Estimate the number of items by assuming 40 characters can fit in a
  // line, and each line contains 3 items; a line box, an inline box, and a
  // text. If it will require more than one reallocations, make an initial
  // reservation here.
  const wtf_size_t estimated_item_count = text_content_.length() / 40 * 3;
  if (UNLIKELY(estimated_item_count > items_.capacity() * 2))
    items_.ReserveInitialCapacity(estimated_item_count);
}

void NGFragmentItemsBuilder::SetCurrentLine(
    const NGPhysicalLineBoxFragment& line,
    ChildList&& children) {
#if DCHECK_IS_ON()
  current_line_fragment_ = &line;
#endif
  current_line_ = std::move(children);
}

void NGFragmentItemsBuilder::AddLine(const NGPhysicalLineBoxFragment& line,
                                     const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);
#if DCHECK_IS_ON()
  DCHECK_EQ(current_line_fragment_, &line);
#endif

  // Reserve the capacity for (children + line box item).
  const wtf_size_t size_before = items_.size();
  const wtf_size_t estimated_size = size_before + current_line_.size() + 1;
  const wtf_size_t old_capacity = items_.capacity();
  if (estimated_size > old_capacity)
    items_.ReserveCapacity(std::max(estimated_size, old_capacity * 2));

  // Add an empty item so that the start of the line can be set later.
  const wtf_size_t line_start_index = items_.size();
  items_.emplace_back(offset);

  AddItems(current_line_.begin(), current_line_.end());

  // All children are added. Create an item for the start of the line.
  const wtf_size_t item_count = items_.size() - line_start_index;
  DCHECK(!items_[line_start_index].item);
  items_[line_start_index].item =
      base::MakeRefCounted<NGFragmentItem>(line, item_count);

  // Keep children's offsets relative to |line|. They will be adjusted later in
  // |ConvertToPhysical()|.

  current_line_.clear();
#if DCHECK_IS_ON()
  current_line_fragment_ = nullptr;
#endif

  DCHECK_LE(items_.size(), estimated_size);
}

void NGFragmentItemsBuilder::AddItems(Child* child_begin, Child* child_end) {
  DCHECK(!is_converted_to_physical_);

  for (Child* child_iter = child_begin; child_iter != child_end;) {
    Child& child = *child_iter;
    // OOF children should have been added to their parent box fragments.
    DCHECK(!child.out_of_flow_positioned_box);
    if (!child.fragment_item) {
      ++child_iter;
      continue;
    }

    if (child.children_count <= 1) {
      items_.emplace_back(std::move(child.fragment_item), child.rect.offset);
      ++child_iter;
      continue;
    }
    DCHECK(child.fragment_item->IsContainer());
    DCHECK(!child.fragment_item->IsFloating());

    // Children of inline boxes are flattened and added to |items_|, with the
    // count of descendant items to preserve the tree structure.
    //
    // Add an empty item so that the start of the box can be set later.
    wtf_size_t box_start_index = items_.size();
    items_.emplace_back(child.rect.offset);

    // Add all children, including their desendants, skipping this item.
    CHECK_GE(child.children_count, 1u);  // 0 will loop infinitely.
    Child* end_child_iter = child_iter + child.children_count;
    CHECK_LE(end_child_iter - child_begin, child_end - child_begin);
    AddItems(child_iter + 1, end_child_iter);
    child_iter = end_child_iter;

    // All children are added. Compute how many items are actually added. The
    // number of items added maybe different from |child.children_count|.
    wtf_size_t item_count = items_.size() - box_start_index;

    // Create an item for the start of the box.
    child.fragment_item->SetDescendantsCount(item_count);
    DCHECK(!items_[box_start_index].item);
    items_[box_start_index].item = std::move(child.fragment_item);
  }
}

void NGFragmentItemsBuilder::AddListMarker(
    const NGPhysicalBoxFragment& marker_fragment,
    const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);

  // Resolved direction matters only for inline items, and outside list markers
  // are not inline.
  const TextDirection resolved_direction = TextDirection::kLtr;
  items_.emplace_back(
      base::MakeRefCounted<NGFragmentItem>(marker_fragment, resolved_direction),
      offset);
}

void NGFragmentItemsBuilder::AddItems(const NGFragmentItems& items,
                                      WritingMode writing_mode,
                                      TextDirection direction,
                                      const PhysicalSize& container_size) {
  DCHECK(items_.IsEmpty());
  const NGFragmentItems::Span source_items = items.Items();
  const wtf_size_t estimated_size = source_items.size();
  items_.ReserveCapacity(estimated_size);

  // Convert offsets to logical. The logic is opposite to |ConvertToPhysical|.
  // This is needed because the container size may be different, in that case,
  // the physical offsets are different when `writing-mode: vertial-rl`.
  DCHECK(!is_converted_to_physical_);
  const WritingMode line_writing_mode = ToLineWritingMode(writing_mode);
  for (NGInlineCursor cursor(items); cursor;) {
    DCHECK(cursor.Current().Item());
    const NGFragmentItem& item = *cursor.Current().Item();
    items_.emplace_back(
        &item, item.OffsetInContainerBlock().ConvertToLogical(
                   writing_mode, direction, container_size, item.Size()));

    if (item.Type() == NGFragmentItem::kLine) {
      const PhysicalRect line_box_bounds = item.RectInContainerBlock();
      for (NGInlineCursor line_children = cursor.CursorForDescendants();
           line_children; line_children.MoveToNext()) {
        const NGFragmentItem& line_child = *line_children.Current().Item();
        items_.emplace_back(
            &line_child,
            (line_child.OffsetInContainerBlock() - line_box_bounds.offset)
                .ConvertToLogical(line_writing_mode, TextDirection::kLtr,
                                  line_box_bounds.size, line_child.Size()));
      }
      cursor.MoveToNextSkippingChildren();
      continue;
    }

    cursor.MoveToNext();
  }
  DCHECK_LE(items_.size(), estimated_size);

  DCHECK(!text_content_);
  text_content_ = items.Text(false);
  first_line_text_content_ = items.Text(true);
}

const NGFragmentItemsBuilder::ItemWithOffsetList& NGFragmentItemsBuilder::Items(
    WritingMode writing_mode,
    TextDirection direction,
    const PhysicalSize& outer_size) {
  ConvertToPhysical(writing_mode, direction, outer_size);
  return items_;
}

// Convert internal logical offsets to physical. Items are kept with logical
// offset until outer box size is determined.
void NGFragmentItemsBuilder::ConvertToPhysical(WritingMode writing_mode,
                                               TextDirection direction,
                                               const PhysicalSize& outer_size) {
  if (is_converted_to_physical_)
    return;

  // Children of lines have line-relative offsets. Use line-writing mode to
  // convert their logical offsets.
  const WritingMode line_writing_mode = ToLineWritingMode(writing_mode);

  for (ItemWithOffset* iter = items_.begin(); iter != items_.end(); ++iter) {
    NGFragmentItem* item = const_cast<NGFragmentItem*>(iter->item.get());
    item->SetOffset(iter->offset.ConvertToPhysical(writing_mode, direction,
                                                   outer_size, item->Size()));

    // Transform children of lines separately from children of the block,
    // because they may have different directions from the block. To do
    // this, their offsets are relative to their containing line box.
    if (item->Type() == NGFragmentItem::kLine) {
      unsigned descendants_count = item->DescendantsCount();
      DCHECK(descendants_count);
      if (descendants_count) {
        const PhysicalRect line_box_bounds = item->RectInContainerBlock();
        while (--descendants_count) {
          ++iter;
          DCHECK_NE(iter, items_.end());
          item = const_cast<NGFragmentItem*>(iter->item.get());
          // Use `kLtr` because inline items are after bidi-reoder, and that
          // their offset is visual, not logical.
          item->SetOffset(iter->offset.ConvertToPhysical(
                              line_writing_mode, TextDirection::kLtr,
                              line_box_bounds.size, item->Size()) +
                          line_box_bounds.offset);
        }
      }
    }
  }

  is_converted_to_physical_ = true;
}

base::Optional<LogicalOffset> NGFragmentItemsBuilder::LogicalOffsetFor(
    const LayoutObject& layout_object) const {
  for (const ItemWithOffset& item : items_) {
    if (item->GetLayoutObject() == &layout_object)
      return item.offset;
  }
  return base::nullopt;
}

void NGFragmentItemsBuilder::ToFragmentItems(WritingMode writing_mode,
                                             TextDirection direction,
                                             const PhysicalSize& outer_size,
                                             void* data) {
  DCHECK(text_content_);
  ConvertToPhysical(writing_mode, direction, outer_size);
  new (data) NGFragmentItems(this);
}

}  // namespace blink
