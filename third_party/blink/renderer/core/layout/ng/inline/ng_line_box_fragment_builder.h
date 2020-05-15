// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class NGInlineBreakToken;
class NGLogicalLineItems;

class CORE_EXPORT NGLineBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode node,
                           scoped_refptr<const ComputedStyle> style,
                           const NGConstraintSpace* space,
                           WritingMode writing_mode,
                           TextDirection)
      : NGContainerFragmentBuilder(node,
                                   style,
                                   space,
                                   writing_mode,
                                   TextDirection::kLtr),
        line_box_type_(NGPhysicalLineBoxFragment::kNormalLineBox),
        base_direction_(TextDirection::kLtr) {}

  void Reset();

  LayoutUnit LineHeight() const {
    return metrics_.LineHeight().ClampNegativeToZero();
  }

  void SetInlineSize(LayoutUnit inline_size) {
    size_.inline_size = inline_size;
  }

  void SetHangInlineSize(LayoutUnit hang_inline_size) {
    hang_inline_size_ = hang_inline_size;
  }

  // Mark this line box is an "empty" line box. See NGLineBoxType.
  void SetIsEmptyLineBox();

  const NGLineHeightMetrics& Metrics() const { return metrics_; }
  void SetMetrics(const NGLineHeightMetrics& metrics) { metrics_ = metrics; }

  void SetBaseDirection(TextDirection direction) {
    base_direction_ = direction;
  }

  // Set the break token for the fragment to build.
  // A finished break token will be attached if not set.
  void SetBreakToken(scoped_refptr<NGInlineBreakToken> break_token) {
    break_token_ = std::move(break_token);
  }

  // Add all items in ChildList. Skips null Child if any.
  void AddChildren(NGLogicalLineItems&);

  // Propagate data in |ChildList| without adding them to this builder. When
  // adding children as fragment items, they appear in the container, but there
  // are some data that should be propagated through line box fragments.
  void PropagateChildrenData(NGLogicalLineItems&);

  // Creates the fragment. Can only be called once.
  scoped_refptr<const NGLayoutResult> ToLineBoxFragment();

 private:
  NGLineHeightMetrics metrics_;
  LayoutUnit hang_inline_size_;
  NGPhysicalLineBoxFragment::NGLineBoxType line_box_type_;
  TextDirection base_direction_;

  friend class NGLayoutResult;
  friend class NGPhysicalLineBoxFragment;

  DISALLOW_COPY_AND_ASSIGN(NGLineBoxFragmentBuilder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_
