// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGFragmentItem;
class NGFragmentItems;

// This class builds |NGFragmentItems|.
//
// Once |NGFragmentItems| is built, it is immutable.
class CORE_EXPORT NGFragmentItemsBuilder {
  STACK_ALLOCATED();

 public:
  NGFragmentItemsBuilder(NGBoxFragmentBuilder* box_builder) {}

  using Child = NGLineBoxFragmentBuilder::Child;
  using ChildList = NGLineBoxFragmentBuilder::ChildList;
  void AddChildren(const ChildList& children);

  void ConvertToPhysical(WritingMode writing_mode,
                         TextDirection direction,
                         PhysicalSize outer_size);

  void ToFragmentItems(void* data);

 private:
  Vector<std::unique_ptr<NGFragmentItem>> items_;
  Vector<LogicalOffset> offsets_;
  String text_content_;
  String first_line_text_content_;

  friend class NGFragmentItems;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
