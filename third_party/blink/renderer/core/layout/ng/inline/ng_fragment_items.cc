// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline bool ShouldSetFirstAndLastForNode() {
  return RuntimeEnabledFeatures::LayoutNGFragmentTraversalEnabled();
}

}  // namespace

NGFragmentItems::NGFragmentItems(NGFragmentItemsBuilder* builder)
    : text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)),
      size_(builder->items_.size()) {
  NGFragmentItemsBuilder::ItemWithOffsetList& source_items = builder->items_;
  for (unsigned i = 0; i < size_; ++i) {
    // Call the move constructor to move without |AddRef|. Items in
    // |NGFragmentItemsBuilder| are not used after |this| was constructed.
    DCHECK(source_items[i].item);
    new (&items_[i])
        scoped_refptr<const NGFragmentItem>(std::move(source_items[i].item));
    DCHECK(!source_items[i].item);  // Ensure the source was moved.
  }
}

NGFragmentItems::~NGFragmentItems() {
  for (unsigned i = 0; i < size_; ++i)
    items_[i]->Release();
}

bool NGFragmentItems::IsSubSpan(const Span& span) const {
  return span.empty() ||
         (span.data() >= ItemsData() && &span.back() < ItemsData() + Size());
}

void NGFragmentItems::FinalizeAfterLayout(
    const Vector<scoped_refptr<const NGLayoutResult>, 1>& results) {
  HashMap<const LayoutObject*, const NGFragmentItem*> first_and_last;
  for (const auto& result : results) {
    const auto& fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    const NGFragmentItems* current = fragment.Items();
    DCHECK(current);
    const Span items = current->Items();
    // items_[0] can be:
    //  - kBox  for list marker, e.g. <li>abc</li>
    //  - kLine for line, e.g. <div>abc</div>
    DCHECK(items.empty() || items[0]->IsContainer());
    if (current->Size() <= 1)
      continue;
    HashMap<const LayoutObject*, wtf_size_t> last_fragment_map;
    wtf_size_t index = 0;
    for (const scoped_refptr<const NGFragmentItem>& item : items.subspan(1)) {
      ++index;
      if (item->Type() == NGFragmentItem::kLine) {
        DCHECK_EQ(item->DeltaToNextForSameLayoutObject(), 0u);
        continue;
      }
      LayoutObject* const layout_object = item->GetMutableLayoutObject();
      if (UNLIKELY(layout_object->IsFloating())) {
        DCHECK_EQ(item->DeltaToNextForSameLayoutObject(), 0u);
        continue;
      }
      DCHECK(!layout_object->IsOutOfFlowPositioned());
      DCHECK(layout_object->IsInLayoutNGInlineFormattingContext()) << *item;
      item->SetDeltaToNextForSameLayoutObject(0);

      if (ShouldSetFirstAndLastForNode()) {
        bool is_first_for_node =
            first_and_last.Set(layout_object, item.get()).is_new_entry;
        item->SetIsFirstForNode(is_first_for_node);
        item->SetIsLastForNode(false);
      }

      // TODO(layout-dev): Make this work for multiple box fragments (block
      // fragmentation).
      if (!fragment.IsFirstForNode())
        continue;

      auto insert_result = last_fragment_map.insert(layout_object, index);
      if (insert_result.is_new_entry) {
        DCHECK_EQ(layout_object->FirstInlineFragmentItemIndex(), 0u);
        layout_object->SetFirstInlineFragmentItemIndex(index);
        continue;
      }
      const wtf_size_t last_index = insert_result.stored_value->value;
      insert_result.stored_value->value = index;
      DCHECK_GT(last_index, 0u) << *item;
      DCHECK_LT(last_index, items.size());
      DCHECK_LT(last_index, index);
      DCHECK_EQ(items[last_index]->DeltaToNextForSameLayoutObject(), 0u);
      items[last_index]->SetDeltaToNextForSameLayoutObject(index - last_index);
    }
  }
  if (!ShouldSetFirstAndLastForNode())
    return;
  for (const auto& iter : first_and_last)
    iter.value->SetIsLastForNode(true);
}

void NGFragmentItems::ClearAssociatedFragments(LayoutObject* container) {
  // Clear by traversing |LayoutObject| tree rather than |NGFragmentItem|
  // because a) we don't need to modify |NGFragmentItem|, and in general the
  // number of |LayoutObject| is less than the number of |NGFragmentItem|.
  for (LayoutObject* child = container->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (UNLIKELY(!child->IsInLayoutNGInlineFormattingContext() ||
                 child->IsFloatingOrOutOfFlowPositioned()))
      continue;
    child->ClearFirstInlineFragmentItemIndex();

    // Children of |LayoutInline| are part of this inline formatting context,
    // but children of other |LayoutObject| (e.g., floats, oof, inline-blocks)
    // are not.
    if (child->IsLayoutInline())
      ClearAssociatedFragments(child);
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeMoved(
    const LayoutObject& layout_object) {
  if (UNLIKELY(layout_object.IsInsideFlowThread())) {
    // TODO(crbug.com/829028): Make NGInlineCursor handle block
    // fragmentation. For now, perform a slow walk here manually.
    const LayoutBlock& container = *layout_object.ContainingBlock();
    for (wtf_size_t idx = 0; idx < container.PhysicalFragmentCount(); idx++) {
      const NGPhysicalBoxFragment& fragment =
          *container.GetPhysicalFragment(idx);
      DCHECK(fragment.Items());
      for (const auto& item : fragment.Items()->Items()) {
        if (item->GetLayoutObject() == &layout_object)
          item->LayoutObjectWillBeMoved();
      }
    }
    return;
  }

  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeMoved();
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  if (UNLIKELY(layout_object.IsInsideFlowThread())) {
    // TODO(crbug.com/829028): Make NGInlineCursor handle block
    // fragmentation. For now, perform a slow walk here manually.
    const LayoutBlock& container = *layout_object.ContainingBlock();
    for (wtf_size_t idx = 0; idx < container.PhysicalFragmentCount(); idx++) {
      const NGPhysicalBoxFragment& fragment =
          *container.GetPhysicalFragment(idx);
      DCHECK(fragment.Items());
      for (const auto& item : fragment.Items()->Items()) {
        if (item->GetLayoutObject() == &layout_object)
          item->LayoutObjectWillBeDestroyed();
      }
    }
    return;
  }

  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeDestroyed();
  }
}

}  // namespace blink
