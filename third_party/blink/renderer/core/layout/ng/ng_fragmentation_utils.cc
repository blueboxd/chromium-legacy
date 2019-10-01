// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

// At a class A break point [1], the break value with the highest precedence
// wins. If the two values have the same precedence (e.g. "left" and "right"),
// the value specified on a latter object wins.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
inline int FragmentainerBreakPrecedence(EBreakBetween break_value) {
  // "auto" has the lowest priority.
  // "avoid*" values win over "auto".
  // "avoid-page" wins over "avoid-column".
  // "avoid" wins over "avoid-page".
  // Forced break values win over "avoid".
  // Any forced page break value wins over "column" forced break.
  // More specific break values (left, right, recto, verso) wins over generic
  // "page" values.

  switch (break_value) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case EBreakBetween::kAuto:
      return 0;
    case EBreakBetween::kAvoidColumn:
      return 1;
    case EBreakBetween::kAvoidPage:
      return 2;
    case EBreakBetween::kAvoid:
      return 3;
    case EBreakBetween::kColumn:
      return 4;
    case EBreakBetween::kPage:
      return 5;
    case EBreakBetween::kLeft:
    case EBreakBetween::kRight:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
      return 6;
  }
}

EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value) {
  if (FragmentainerBreakPrecedence(second_value) >=
      FragmentainerBreakPrecedence(first_value))
    return second_value;
  return first_value;
}

bool IsForcedBreakValue(const NGConstraintSpace& constraint_space,
                        EBreakBetween break_value) {
  if (break_value == EBreakBetween::kColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to force a break to the next page, even if we're in
  // multicol (printing multicol, for instance).
  if (break_value == EBreakBetween::kLeft ||
      break_value == EBreakBetween::kPage ||
      break_value == EBreakBetween::kRecto ||
      break_value == EBreakBetween::kRight ||
      break_value == EBreakBetween::kVerso)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}

bool IsAvoidBreakValue(const NGConstraintSpace& constraint_space,
                       EBreakInside break_value) {
  if (break_value == EBreakInside::kAvoid)
    return constraint_space.HasBlockFragmentation();
  if (break_value == EBreakInside::kAvoidColumn)
    return constraint_space.BlockFragmentationType() == kFragmentColumn;
  // TODO(mstensho): The innermost fragmentation type doesn't tell us everything
  // here. We might want to avoid breaking to the next page, even if we're
  // in multicol (printing multicol, for instance).
  if (break_value == EBreakInside::kAvoidPage)
    return constraint_space.BlockFragmentationType() == kFragmentPage;
  return false;
}

EBreakBetween CalculateBreakBetweenValue(NGLayoutInputNode child,
                                         const NGLayoutResult& layout_result,
                                         const NGBoxFragmentBuilder& builder) {
  if (child.IsInline())
    return EBreakBetween::kAuto;
  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  return builder.JoinedBreakBetweenValue(break_before);
}

void SetupFragmentation(const NGConstraintSpace& parent_space,
                        LayoutUnit new_bfc_block_offset,
                        NGConstraintSpaceBuilder* builder,
                        bool is_new_fc) {
  DCHECK(parent_space.HasBlockFragmentation());

  LayoutUnit space_available =
      parent_space.FragmentainerSpaceAtBfcStart() - new_bfc_block_offset;

  builder->SetFragmentainerBlockSize(parent_space.FragmentainerBlockSize());
  builder->SetFragmentainerSpaceAtBfcStart(space_available);
  builder->SetFragmentationType(parent_space.BlockFragmentationType());

  if (parent_space.IsInColumnBfc() && !is_new_fc)
    builder->SetIsInColumnBfc();
}

void FinishFragmentation(NGBoxFragmentBuilder* builder,
                         LayoutUnit block_size,
                         LayoutUnit intrinsic_block_size,
                         LayoutUnit previously_consumed_block_size,
                         LayoutUnit space_left) {
  if (builder->DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space, we
    // need to prepare a break token.
    builder->SetConsumedBlockSize(std::min(space_left, block_size) +
                                  previously_consumed_block_size);
    builder->SetBlockSize(std::min(space_left, block_size));
    builder->SetIntrinsicBlockSize(space_left);
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    builder->SetConsumedBlockSize(space_left + previously_consumed_block_size);
    builder->SetDidBreak();
    builder->SetBlockSize(space_left);
    builder->SetIntrinsicBlockSize(space_left);
    builder->PropagateSpaceShortage(block_size - space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  builder->SetConsumedBlockSize(previously_consumed_block_size + block_size);
  builder->SetBlockSize(block_size);
  builder->SetIntrinsicBlockSize(intrinsic_block_size);
}

}  // namespace blink
