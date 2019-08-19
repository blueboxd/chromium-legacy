// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGFragmentItem::NGFragmentItem(const NGPhysicalTextFragment& text)
    : layout_object_(text.GetLayoutObject()),
      text_({text.TextShapeResult(), text.StartOffset(), text.EndOffset()}),
      size_(text.Size()),
      type_(kText),
      style_variant_(static_cast<unsigned>(text.StyleVariant())) {
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), text_.start_offset);
    DCHECK_EQ(text_.shape_result->EndIndex(), text_.end_offset);
  }
#endif
}

NGFragmentItem::NGFragmentItem(const NGPhysicalLineBoxFragment& line,
                               wtf_size_t item_count)
    : layout_object_(nullptr),
      line_({line.Metrics(), To<NGInlineBreakToken>(line.BreakToken()),
             item_count}),
      size_(line.Size()),
      type_(kLine),
      style_variant_(static_cast<unsigned>(line.StyleVariant())) {}

NGFragmentItem::NGFragmentItem(const NGPhysicalBoxFragment& box,
                               wtf_size_t item_count)
    : layout_object_(box.GetLayoutObject()),
      box_({&box, item_count}),
      size_(box.Size()),
      type_(kBox),
      style_variant_(static_cast<unsigned>(box.StyleVariant())) {}

NGFragmentItem::~NGFragmentItem() {
  switch (Type()) {
    case kText:
      text_.~Text();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedText();
      break;
    case kLine:
      line_.~Line();
      break;
    case kBox:
      box_.~Box();
      break;
  }
}

String NGFragmentItem::DebugName() const {
  return "NGFragmentItem";
}

IntRect NGFragmentItem::VisualRect() const {
  // TODO(kojii): Implement.
  return IntRect();
}

}  // namespace blink
