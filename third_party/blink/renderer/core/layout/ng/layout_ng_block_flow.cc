// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutNGBlockFlowMixin<LayoutBlockFlow>(element) {}

LayoutNGBlockFlow::~LayoutNGBlockFlow() = default;

bool LayoutNGBlockFlow::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGBlockFlow ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

void LayoutNGBlockFlow::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  scoped_refptr<const NGLayoutResult> result =
      NGBlockNode(this).Layout(constraint_space);

  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();

  UpdateMargins(constraint_space);
}

void LayoutNGBlockFlow::UpdateMargins(const NGConstraintSpace& space) {
  const LayoutBlock* containing_block = ContainingBlock();
  if (!containing_block || !containing_block->IsLayoutBlockFlow())
    return;

  // In the legacy engine, for regular block container layout, children
  // calculate and store margins on themselves, while in NG that's done by the
  // container. Since this object is a LayoutNG entry-point, we'll have to do it
  // on ourselves, since that's what the legacy container expects.
  const ComputedStyle& style = StyleRef();
  const ComputedStyle& cb_style = containing_block->StyleRef();
  const auto writing_mode = cb_style.GetWritingMode();
  const auto direction = cb_style.Direction();
  LayoutUnit percentage_resolution_size =
      space.PercentageResolutionInlineSizeForParentWritingMode();
  NGBoxStrut margins = ComputePhysicalMargins(style, percentage_resolution_size)
                           .ConvertToLogical(writing_mode, direction);
  ResolveInlineMargins(style, cb_style, space.AvailableSize().inline_size,
                       LogicalWidth(), &margins);
  SetMargin(margins.ConvertToPhysical(writing_mode, direction));
}

}  // namespace blink
