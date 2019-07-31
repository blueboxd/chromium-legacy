// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

namespace blink {

void NGFragmentItemsBuilder::AddChildren(const ChildList& children) {
  items_.ReserveCapacity(items_.size() + children.size());
  offsets_.ReserveCapacity(items_.size() + children.size());

  for (auto& child : children) {
    if (child.fragment) {
      items_.push_back(std::make_unique<NGFragmentItem>(
          *child.fragment->GetLayoutObject(), child.fragment->StyleVariant(),
          NGFragmentItem::Text{child.fragment->TextShapeResult()}));
      offsets_.push_back(child.offset);
      continue;
    }

    // TODO(kojii): Implement other cases.
  }
}

void NGFragmentItemsBuilder::ConvertToPhysical(WritingMode writing_mode,
                                               TextDirection direction,
                                               PhysicalSize outer_size) {
  CHECK_EQ(items_.size(), offsets_.size());
  const LogicalOffset* offset_iter = offsets_.begin();
  for (auto& item : items_) {
    PhysicalOffset offset = offset_iter->ConvertToPhysical(
        writing_mode, direction, outer_size, item->Size());
    item->SetOffset(offset);
    ++offset_iter;
  }
}

void NGFragmentItemsBuilder::ToFragmentItems(void* data) {
  new (data) NGFragmentItems(this);
}

}  // namespace blink
