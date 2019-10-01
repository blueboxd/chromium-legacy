// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentationUtils_h
#define NGFragmentationUtils_h

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGConstraintSpace;
class NGLayoutResult;

// Join two adjacent break values specified on break-before and/or break-
// after. avoid* values win over auto values, and forced break values win over
// avoid* values. |first_value| is specified on an element earlier in the flow
// than |second_value|. This method is used at class A break points [1], to join
// the values of the previous break-after and the next break-before, to figure
// out whether we may, must, or should not break at that point. It is also used
// when propagating break-before values from first children and break-after
// values on last children to their container.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
EBreakBetween JoinFragmentainerBreakValues(EBreakBetween first_value,
                                           EBreakBetween second_value);

// Return true if the specified break value has a forced break effect in the
// current fragmentation context.
bool IsForcedBreakValue(const NGConstraintSpace&, EBreakBetween);

// Return true if the specified break value means that we should avoid breaking,
// given the current fragmentation context.
bool IsAvoidBreakValue(const NGConstraintSpace&, EBreakInside);

// Return true if we're resuming layout after a previous break.
inline bool IsResumingLayout(const NGBlockBreakToken* token) {
  return token && !token->IsBreakBefore();
}

// Calculate the final "break-between" value at a class A or C breakpoint. This
// is the combination of all break-before and break-after values that met at the
// breakpoint.
EBreakBetween CalculateBreakBetweenValue(NGLayoutInputNode child,
                                         const NGLayoutResult&,
                                         const NGBoxFragmentBuilder&);

// Set up a child's constraint space builder for block fragmentation. The child
// participates in the same fragmentation context as parent_space. If the child
// establishes a new formatting context, new_bfc_block_offset must be set to the
// offset from the parent block formatting context, or, if the parent formatting
// context starts in a previous fragmentainer; the offset from the current
// fragmentainer block-start.
void SetupFragmentation(const NGConstraintSpace& parent_space,
                        LayoutUnit new_bfc_block_offset,
                        NGConstraintSpaceBuilder*,
                        bool is_new_fc);

// Write fragmentation information to the fragment builder after layout.
void FinishFragmentation(NGBoxFragmentBuilder*,
                         LayoutUnit block_size,
                         LayoutUnit intrinsic_block_size,
                         LayoutUnit previously_consumed_block_size,
                         LayoutUnit space_left);

}  // namespace blink

#endif  // NGFragmentationUtils_h
