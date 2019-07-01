// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutNGMixin<LayoutBlockFlow>(element) {}

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

void LayoutNGBlockFlow::UpdateOutOfFlowBlockLayout() {
  LayoutBoxModelObject* css_container = ToLayoutBoxModelObject(Container());
  LayoutBox* container =
      css_container->IsBox() ? ToLayoutBox(css_container) : ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // As this is part of the Legacy->NG bridge, the container_builder is used
  // for indicating the resolved size of the OOF-positioned containing-block
  // and not used for caching purposes.
  // When we produce a layout result from it, we access its child fragments
  // which must contain *at least* this node. We use the child fragments for
  // copying back position information.
  NGBlockNode container_node(container);
  NGBoxFragmentBuilder container_builder(
      container_node, scoped_refptr<const ComputedStyle>(container_style),
      /* space */ nullptr, container_style->GetWritingMode(),
      container_style->Direction());
  container_builder.SetIsNewFormattingContext(
      container_node.CreatesNewFormattingContext());

  NGFragmentGeometry fragment_geometry;
  fragment_geometry.border = ComputeBorders(constraint_space, container_node);
  fragment_geometry.scrollbar =
      ComputeScrollbars(constraint_space, container_node);
  fragment_geometry.padding =
      ComputePadding(constraint_space, *container_style);

  NGBoxStrut border_scrollbar =
      fragment_geometry.border + fragment_geometry.scrollbar;

  // Calculate the border-box size of the object that's the containing block of
  // this out-of-flow positioned descendant. Note that this is not to be used as
  // the containing block size to resolve sizes and positions for the
  // descendant, since we're dealing with the border box here (not the padding
  // box, which is where the containing block is established). These sizes are
  // just used to do a fake/partial NG layout pass of the containing block (that
  // object is really managed by legacy layout).
  LayoutUnit container_border_box_logical_width;
  LayoutUnit container_border_box_logical_height;
  if (HasOverrideContainingBlockContentLogicalWidth()) {
    container_border_box_logical_width =
        OverrideContainingBlockContentLogicalWidth() +
        border_scrollbar.InlineSum();
  } else {
    container_border_box_logical_width = container->LogicalWidth();
  }
  if (HasOverrideContainingBlockContentLogicalHeight()) {
    container_border_box_logical_height =
        OverrideContainingBlockContentLogicalHeight() +
        border_scrollbar.BlockSum();
  } else {
    container_border_box_logical_height = container->LogicalHeight();
  }

  fragment_geometry.border_box_size = {container_border_box_logical_width,
                                       container_border_box_logical_height};
  container_builder.SetInitialFragmentGeometry(fragment_geometry);

  NGLogicalStaticPosition static_position =
      LayoutBoxUtils::ComputeStaticPositionFromLegacy(*this, border_scrollbar);
  // Set correct container for inline containing blocks.
  container_builder.AddOutOfFlowLegacyCandidate(
      NGBlockNode(this), static_position, ToLayoutInlineOrNull(css_container));

  base::Optional<LogicalSize> initial_containing_block_fixed_size;
  if (container->IsLayoutView() && !GetDocument().Printing()) {
    if (LocalFrameView* frame_view = ToLayoutView(container)->GetFrameView()) {
      IntSize size =
          frame_view->LayoutViewport()->ExcludeScrollbars(frame_view->Size());
      PhysicalSize physical_size(size);
      initial_containing_block_fixed_size =
          physical_size.ConvertToLogical(container->Style()->GetWritingMode());
    }
  }
  // We really only want to lay out ourselves here, so we pass |this| to
  // Run(). Otherwise, NGOutOfFlowLayoutPart may also lay out other objects
  // it discovers that are part of the same containing block, but those
  // should get laid out by the actual containing block.
  NGOutOfFlowLayoutPart(css_container->CanContainAbsolutePositionObjects(),
                        css_container->CanContainFixedPositionObjects(),
                        *container_style, constraint_space, border_scrollbar,
                        &container_builder, initial_containing_block_fixed_size)
      .Run(/* only_layout */ this);
  scoped_refptr<const NGLayoutResult> result =
      container_builder.ToBoxFragment();
  // These are the unpositioned OOF descendants of the current OOF block.
  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();

  const auto& fragment = result->PhysicalFragment();
  DCHECK_GT(fragment.Children().size(), 0u);
  // Copy sizes of all child fragments to Legacy.
  // There could be multiple fragments, when this node has descendants whose
  // container is this node's container.
  // Example: fixed descendant of fixed element.
  for (auto& child : fragment.Children()) {
    const NGPhysicalFragment* child_fragment = child.get();
    DCHECK(child_fragment->GetLayoutObject()->IsBox());
    LayoutBox* child_legacy_box =
        ToLayoutBox(child_fragment->GetMutableLayoutObject());
    PhysicalOffset child_offset = child.Offset();
    if (container_style->IsFlippedBlocksWritingMode()) {
      child_legacy_box->SetX(container_border_box_logical_height -
                             child_offset.left - child_fragment->Size().width);
    } else {
      child_legacy_box->SetX(child_offset.left);
    }
    child_legacy_box->SetY(child_offset.top);
  }
  DCHECK_EQ(fragment.Children()[0]->GetLayoutObject(), this);
  SetIsLegacyInitiatedOutOfFlowLayout(true);
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
