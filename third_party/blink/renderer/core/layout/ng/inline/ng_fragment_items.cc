// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

namespace blink {

NGFragmentItems::NGFragmentItems(NGFragmentItemsBuilder* builder)
    : items_(std::move(builder->items_)),
      text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)) {}

// static
void NGFragmentItems::AssociateWithLayoutObject(
    Vector<std::unique_ptr<NGFragmentItem>>* items) {
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
    if (item.Type() == NGFragmentItem::kLine)
      continue;
    LayoutObject* const layout_object = item.GetMutableLayoutObject();
    DCHECK(layout_object->IsInLayoutNGInlineFormattingContext()) << item;
    auto insert_result = last_fragment_map.insert(layout_object, index);
    if (insert_result.is_new_entry) {
      layout_object->SetFirstInlineFragmentItemIndex(index);
      continue;
    }
    const wtf_size_t last_index = insert_result.stored_value->value;
    insert_result.stored_value->value = index;
    DCHECK_GT(last_index, 0u) << item;
    DCHECK_LT(last_index, items->size());
    DCHECK_LT(last_index, index);
    (*items)[last_index]->SetDeltaToNextForSameLayoutObject(index - last_index);
  }
}

}  // namespace blink
