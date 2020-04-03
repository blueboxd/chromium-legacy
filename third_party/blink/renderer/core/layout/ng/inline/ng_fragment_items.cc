// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

namespace blink {

namespace {

inline bool ShouldAssociateWithLayoutObject(const NGFragmentItem& item) {
  return item.Type() != NGFragmentItem::kLine && !item.IsFloating();
}

}  // namespace

NGFragmentItems::NGFragmentItems(NGFragmentItemsBuilder* builder)
    : items_(std::move(builder->items_)),
      text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)) {}

void NGFragmentItems::AssociateWithLayoutObject() const {
  const Vector<std::unique_ptr<NGFragmentItem>>* items = &items_;
  DCHECK(std::all_of(items->begin(), items->end(), [](const auto& item) {
    return !item->DeltaToNextForSameLayoutObject();
  }));
  // items_[0] can be:
  //  - kBox  for list marker, e.g. <li>abc</li>
  //  - kLine for line, e.g. <div>abc</div>
  // Calling get() is necessary below because operator<< in std::unique_ptr is
  // a C++20 feature.
  // TODO(https://crbug.com/980914): Drop .get() once we move to C++20.
  DCHECK(items->IsEmpty() || (*items)[0]->IsContainer()) << (*items)[0].get();
  HashMap<const LayoutObject*, wtf_size_t> last_fragment_map;
  for (wtf_size_t index = 1u; index < items->size(); ++index) {
    const NGFragmentItem& item = *(*items)[index];
    if (!ShouldAssociateWithLayoutObject(item))
      continue;
    LayoutObject* const layout_object = item.GetMutableLayoutObject();
    DCHECK(layout_object->IsInLayoutNGInlineFormattingContext()) << item;
    auto insert_result = last_fragment_map.insert(layout_object, index);
    if (insert_result.is_new_entry) {
      DCHECK_EQ(layout_object->FirstInlineFragmentItemIndex(), 0u);
      layout_object->SetFirstInlineFragmentItemIndex(index);
      continue;
    }
    const wtf_size_t last_index = insert_result.stored_value->value;
    insert_result.stored_value->value = index;
    DCHECK_GT(last_index, 0u) << item;
    DCHECK_LT(last_index, items->size());
    DCHECK_LT(last_index, index);
    DCHECK_EQ((*items)[last_index]->DeltaToNextForSameLayoutObject(), 0u);
    (*items)[last_index]->SetDeltaToNextForSameLayoutObject(index - last_index);
  }
}

void NGFragmentItems::ClearAssociatedFragments() const {
  DCHECK(items_.IsEmpty() || items_[0]->IsContainer());
  if (items_.size() <= 1)
    return;
  LayoutObject* last_object = nullptr;
  for (const auto& item : base::span<const std::unique_ptr<NGFragmentItem>>(
           items_.begin() + 1, items_.end())) {
    if (!ShouldAssociateWithLayoutObject(*item)) {
      // These items are not associated and that no need to clear.
      continue;
    }
    LayoutObject* object = item->GetMutableLayoutObject();
    if (!object || object == last_object)
      continue;
    if (object->IsInLayoutNGInlineFormattingContext())
      object->ClearFirstInlineFragmentItemIndex();
    last_object = object;
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeMoved(
    const LayoutObject& layout_object) {
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
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeDestroyed();
  }
}

}  // namespace blink
