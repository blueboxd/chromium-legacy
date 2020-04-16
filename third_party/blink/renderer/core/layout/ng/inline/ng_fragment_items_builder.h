// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"

namespace blink {

class NGFragmentItem;
class NGFragmentItems;
class NGInlineNode;

// This class builds |NGFragmentItems|.
//
// Once |NGFragmentItems| is built, it is immutable.
class CORE_EXPORT NGFragmentItemsBuilder {
  STACK_ALLOCATED();

 public:
  NGFragmentItemsBuilder() = default;
  explicit NGFragmentItemsBuilder(const NGInlineNode& node);

  wtf_size_t Size() const { return items_.size(); }

  // Returns true if we have any floating descendants which need to be
  // traversed during the float paint phase.
  bool HasFloatingDescendantsForPaint() const {
    return has_floating_descendants_for_paint_;
  }

  const String& TextContent(bool first_line) const {
    return UNLIKELY(first_line && first_line_text_content_)
               ? first_line_text_content_
               : text_content_;
  }

  // The caller should create a |ChildList| for a complete line and add to this
  // builder.
  //
  // Adding a line is a two-pass operation, because |NGInlineLayoutAlgorithm|
  // creates and positions children within a line box, but its parent algorithm
  // positions the line box. |SetCurrentLine| sets the children, and the next
  // |AddLine| adds them.
  //
  // TODO(kojii): Moving |ChildList| is not cheap because it has inline
  // capacity. Reconsider the ownership.
  using Child = NGLineBoxFragmentBuilder::Child;
  using ChildList = NGLineBoxFragmentBuilder::ChildList;
  void SetCurrentLine(const NGPhysicalLineBoxFragment& line,
                      ChildList&& children);
  void AddLine(const NGPhysicalLineBoxFragment& line,
               const LogicalOffset& offset);

  // Add a list marker to the current line.
  void AddListMarker(const NGPhysicalBoxFragment& marker_fragment,
                     const LogicalOffset& offset);

  // Add previously laid out |NGFragmentItems|.
  void AddItems(const NGFragmentItems& items,
                WritingMode writing_mode,
                TextDirection direction,
                const PhysicalSize& container_size);

  struct ItemWithOffset {
    DISALLOW_NEW();

   public:
    ItemWithOffset(scoped_refptr<const NGFragmentItem> item,
                   const LogicalOffset& offset)
        : item(std::move(item)), offset(offset) {}
    explicit ItemWithOffset(const LogicalOffset& offset) : offset(offset) {}

    const NGFragmentItem& operator*() const { return *item; }
    const NGFragmentItem* operator->() const { return item.get(); }

    scoped_refptr<const NGFragmentItem> item;
    LogicalOffset offset;
  };

  // Give an inline size, the allocation of this vector is hot. "128" is
  // heuristic. Usually 10-40, some wikipedia pages have >64 items.
  using ItemWithOffsetList = Vector<ItemWithOffset, 128>;

  // Find |LogicalOffset| of the first |NGFragmentItem| for |LayoutObject|.
  base::Optional<LogicalOffset> LogicalOffsetFor(const LayoutObject&) const;

  // Converts the |NGFragmentItem| vector to the physical coordinate space and
  // returns the result. This should only be used for determining the inline
  // containing block geometry for OOF-positioned nodes.
  //
  // Once this method has been called, new items cannot be added.
  const ItemWithOffsetList& Items(WritingMode,
                                  TextDirection,
                                  const PhysicalSize& outer_size);

  // Build a |NGFragmentItems|. The builder cannot build twice because data set
  // to this builder may be cleared.
  void ToFragmentItems(WritingMode,
                       TextDirection,
                       const PhysicalSize& outer_size,
                       void* data);

 private:
  void AddItems(Child* child_begin, Child* child_end);

  void ConvertToPhysical(WritingMode writing_mode,
                         TextDirection direction,
                         const PhysicalSize& outer_size);

  ItemWithOffsetList items_;
  String text_content_;
  String first_line_text_content_;

  // Keeps children of a line until the offset is determined. See |AddLine|.
  ChildList current_line_;

  bool has_floating_descendants_for_paint_ = false;
  bool is_converted_to_physical_ = false;

#if DCHECK_IS_ON()
  const NGPhysicalLineBoxFragment* current_line_fragment_ = nullptr;
#endif

  friend class NGFragmentItems;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
