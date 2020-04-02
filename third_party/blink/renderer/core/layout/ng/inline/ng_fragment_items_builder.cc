// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

void NGFragmentItemsBuilder::SetTextContent(const NGInlineNode& node) {
  const NGInlineItemsData& items_data = node.ItemsData(false);
  text_content_ = items_data.text_content;
  const NGInlineItemsData& first_line = node.ItemsData(true);
  if (&items_data != &first_line)
    first_line_text_content_ = first_line.text_content;
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
  DCHECK_EQ(items_.size(), offsets_.size());
  DCHECK(!is_converted_to_physical_);
#if DCHECK_IS_ON()
  DCHECK_EQ(current_line_fragment_, &line);
#endif

  // Reserve the capacity for (children + line box item).
  wtf_size_t size_before = items_.size();
  wtf_size_t capacity = size_before + current_line_.size() + 1;
  items_.ReserveCapacity(capacity);
  offsets_.ReserveCapacity(capacity);

  // Add an empty item so that the start of the line can be set later.
  wtf_size_t line_start_index = items_.size();
  items_.Grow(line_start_index + 1);
  offsets_.push_back(offset);

  AddItems(current_line_.begin(), current_line_.end());

  // All children are added. Create an item for the start of the line.
  wtf_size_t item_count = items_.size() - line_start_index;
  items_[line_start_index] = std::make_unique<NGFragmentItem>(line, item_count);
  // TODO(kojii): We probably need an end marker too for the reverse-order
  // traversals.

  // Keep children's offsets relative to |line|. They will be adjusted later in
  // |ConvertToPhysical()|.

  current_line_.clear();
#if DCHECK_IS_ON()
  current_line_fragment_ = nullptr;
#endif
}

void NGFragmentItemsBuilder::AddItems(Child* child_begin, Child* child_end) {
  DCHECK_EQ(items_.size(), offsets_.size());
  DCHECK(!is_converted_to_physical_);

  for (Child* child_iter = child_begin; child_iter != child_end;) {
    Child& child = *child_iter;
    if (const NGPhysicalTextFragment* text = child.fragment.get()) {
      items_.push_back(std::make_unique<NGFragmentItem>(*text));
      offsets_.push_back(child.rect.offset);
      ++child_iter;
      continue;
    }

    if (child.layout_result || child.inline_item) {
      // Create an item if this box has no inline children.
      std::unique_ptr<NGFragmentItem> item;
      if (child.layout_result) {
        const NGPhysicalBoxFragment& box =
            To<NGPhysicalBoxFragment>(child.layout_result->PhysicalFragment());
        item = std::make_unique<NGFragmentItem>(box, child.ResolvedDirection());
      } else {
        DCHECK(child.inline_item);
        item = std::make_unique<NGFragmentItem>(
            *child.inline_item,
            ToPhysicalSize(child.rect.size,
                           child.inline_item->Style()->GetWritingMode()));
      }

      // Take the fast path when we know |child| does not have child items.
      if (child.children_count <= 1) {
        items_.push_back(std::move(item));
        offsets_.push_back(child.rect.offset);
        ++child_iter;
        continue;
      }
      DCHECK(!item->IsFloating());

      // Children of inline boxes are flattened and added to |items_|, with the
      // count of descendant items to preserve the tree structure.
      //
      // Add an empty item so that the start of the box can be set later.
      wtf_size_t box_start_index = items_.size();
      items_.Grow(box_start_index + 1);
      offsets_.push_back(child.rect.offset);

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
      item->SetDescendantsCount(item_count);
      items_[box_start_index] = std::move(item);
      continue;
    }

    // OOF children should have been added to their parent box fragments.
    // TODO(kojii): Consider handling them in NGFragmentItem too.
    DCHECK(!child.out_of_flow_positioned_box);
    ++child_iter;
  }
}

void NGFragmentItemsBuilder::AddListMarker(
    const NGPhysicalBoxFragment& marker_fragment,
    const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);

  // Resolved direction matters only for inline items, and outside list markers
  // are not inline.
  const TextDirection resolved_direction = TextDirection::kLtr;
  items_.push_back(
      std::make_unique<NGFragmentItem>(marker_fragment, resolved_direction));
  offsets_.push_back(offset);
}

const Vector<std::unique_ptr<NGFragmentItem>>& NGFragmentItemsBuilder::Items(
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
  CHECK_EQ(items_.size(), offsets_.size());
  if (is_converted_to_physical_)
    return;

  // Children of lines have line-relative offsets. Use line-writing mode to
  // convert their logical offsets.
  const WritingMode line_writing_mode = ToLineWritingMode(writing_mode);

  std::unique_ptr<NGFragmentItem>* item_iter = items_.begin();
  const LogicalOffset* offset = offsets_.begin();
  for (; item_iter != items_.end(); ++item_iter, ++offset) {
    DCHECK_NE(offset, offsets_.end());
    NGFragmentItem* item = item_iter->get();
    item->SetOffset(offset->ConvertToPhysical(writing_mode, direction,
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
          ++offset;
          ++item_iter;
          DCHECK_NE(offset, offsets_.end());
          DCHECK_NE(item_iter, items_.end());
          item = item_iter->get();
          // Use `kLtr` because inline items are after bidi-reoder, and that
          // their offset is visual, not logical.
          item->SetOffset(
              offset->ConvertToPhysical(line_writing_mode, TextDirection::kLtr,
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
  DCHECK_EQ(items_.size(), offsets_.size());
  for (const std::unique_ptr<NGFragmentItem>& item : items_) {
    if (item->GetLayoutObject() == &layout_object)
      return offsets_[&item - items_.begin()];
  }
  return base::nullopt;
}

void NGFragmentItemsBuilder::ToFragmentItems(WritingMode writing_mode,
                                             TextDirection direction,
                                             const PhysicalSize& outer_size,
                                             void* data) {
  ConvertToPhysical(writing_mode, direction, outer_size);
  NGFragmentItems::AssociateWithLayoutObject(&items_);
  new (data) NGFragmentItems(this);
}

}  // namespace blink
