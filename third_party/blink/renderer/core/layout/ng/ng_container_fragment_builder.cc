// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddChild(
    const NGPhysicalContainerFragment& child,
    const LogicalOffset& child_offset) {
  // Collect the child's out of flow descendants.
  // child_offset is offset of inline_start/block_start vertex.
  // Candidates need offset of top/left vertex.
  if (child.HasOutOfFlowPositionedDescendants()) {
    const auto& out_of_flow_descendants =
        child.OutOfFlowPositionedDescendants();
    LogicalOffset top_left_offset;
    PhysicalSize child_size = child.Size();
    switch (GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        top_left_offset =
            (IsRtl(Direction()))
                ? LogicalOffset{child_offset.inline_offset + child_size.width,
                                child_offset.block_offset}
                : child_offset;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        top_left_offset =
            (IsRtl(Direction()))
                ? LogicalOffset{child_offset.inline_offset + child_size.height,
                                child_offset.block_offset + child_size.width}
                : LogicalOffset{child_offset.inline_offset,
                                child_offset.block_offset + child_size.width};
        break;
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        top_left_offset =
            (IsRtl(Direction()))
                ? LogicalOffset{child_offset.inline_offset + child_size.height,
                                child_offset.block_offset}
                : child_offset;
        break;
    }

    // We can end up in a case where we need to account for the relative
    // position of an element to correctly determine the static position of a
    // descendant. E.g.
    // <div id="fixed_container">
    //   <div style="position: relative; top: 10px;">
    //     <div style="position: fixed;"></div>
    //   </div>
    // </div>
    // TODO(layout-dev): This code should eventually be removed once we handle
    // relative positioned objects directly in the fragment tree.
    if (const LayoutBox* child_box =
            ToLayoutBoxOrNull(child.GetLayoutObject())) {
      top_left_offset += PhysicalOffset(child_box->OffsetForInFlowPosition())
                             .ConvertToLogical(GetWritingMode(), Direction(),
                                               PhysicalSize(), PhysicalSize());
    }

    for (const NGOutOfFlowPositionedDescendant& descendant :
         out_of_flow_descendants) {
      oof_positioned_candidates_.push_back(
          NGOutOfFlowPositionedCandidate(descendant, top_left_offset));
    }
  }

  // For the |has_orthogonal_flow_roots_| flag, we don't care about the type of
  // child (OOF-positioned, etc), it is for *any* descendant.
  if (child.HasOrthogonalFlowRoots() ||
      !IsParallelWritingMode(child.Style().GetWritingMode(),
                             Style().GetWritingMode()))
    has_orthogonal_flow_roots_ = true;

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
    has_descendant_that_depends_on_percentage_block_size_ = true;

  // The |may_have_descendant_above_block_start_| flag is used to determine if
  // a fragment can be re-used when floats are present. We only are about:
  //  - Inflow children who are positioned above our block-start edge.
  //  - Any inflow descendants (within the same formatting-context) that *may*
  //    be positioned above our block-start edge.
  if ((child_offset.block_offset < LayoutUnit() &&
       !child.IsOutOfFlowPositioned()) ||
      (!child.IsBlockFormattingContextRoot() &&
       child.MayHaveDescendantAboveBlockStart()))
    may_have_descendant_above_block_start_ = true;

  // Compute |has_floating_descendants_| to optimize tree traversal in paint.
  if (!has_floating_descendants_) {
    if (child.IsFloating()) {
      has_floating_descendants_ = true;
    } else {
      if (!child.IsBlockFormattingContextRoot() &&
          child.HasFloatingDescendants())
        has_floating_descendants_ = true;
    }
  }

  // Collect any (block) break tokens.
  NGBreakToken* child_break_token = child.BreakToken();
  if (child_break_token && has_block_fragmentation_) {
    switch (child.Type()) {
      case NGPhysicalFragment::kFragmentBox:
      case NGPhysicalFragment::kFragmentRenderedLegend:
        if (To<NGBlockBreakToken>(child_break_token)->HasLastResortBreak())
          has_last_resort_break_ = true;
        child_break_tokens_.push_back(child_break_token);
        break;
      case NGPhysicalFragment::kFragmentLineBox:
        // NGInlineNode produces multiple line boxes in an anonymous box. We
        // won't know up front which line box to insert a fragment break before
        // (due to widows), so keep them all until we know.
        inline_break_tokens_.push_back(child_break_token);
        break;
      case NGPhysicalFragment::kFragmentText:
      default:
        NOTREACHED();
        break;
    }
  }

  AddChildInternal(&child, child_offset);
  return *this;
}

void NGContainerFragmentBuilder::AddChildInternal(
    scoped_refptr<const NGPhysicalFragment> child,
    const LogicalOffset& child_offset) {
  // In order to know where list-markers are within the children list (for the
  // |NGSimplifiedLayoutAlgorithm|) we always place them as the first child.
  if (child->IsListMarker()) {
    children_.push_front(ChildWithOffset(child_offset, std::move(child)));
    return;
  }

  children_.emplace_back(child_offset, std::move(child));
}

LogicalOffset NGContainerFragmentBuilder::GetChildOffset(
    const LayoutObject* object) const {
  for (const auto& child : children_) {
    if (child.fragment->GetLayoutObject() == object)
      return child.offset;

    // TODO(layout-dev): ikilpatrick thinks we may need to traverse
    // further than the initial line-box children for a nested inline
    // container. We could not come up with a testcase, it would be
    // something with split inlines, and nested oof/fixed descendants maybe.
    if (child.fragment->IsLineBox()) {
      const auto& line_box_fragment =
          To<NGPhysicalLineBoxFragment>(*child.fragment);
      for (const auto& line_box_child : line_box_fragment.Children()) {
        if (line_box_child->GetLayoutObject() == object) {
          return child.offset + line_box_child.Offset().ConvertToLogical(
                                    GetWritingMode(), Direction(),
                                    line_box_fragment.Size(),
                                    line_box_child->Size());
        }
      }
    }
  }
  NOTREACHED();
  return LogicalOffset();
}

NGContainerFragmentBuilder&
NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    base::Optional<TextDirection> container_direction) {
  DCHECK(child);
  DCHECK(layout_object_ && !layout_object_->IsLayoutInline() ||
         container_direction)
      << "container_direction must only be set for inline-level OOF children.";

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  TextDirection direction = container_direction.value_or(Direction());

  oof_positioned_candidates_.push_back(NGOutOfFlowPositionedCandidate(
      NGOutOfFlowPositionedDescendant(
          child, NGStaticPosition::Create(GetWritingMode(), direction,
                                          PhysicalOffset())),
      child_offset));

  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    NGOutOfFlowPositionedDescendant descendant) {
  oof_positioned_descendants_.push_back(descendant);
  return *this;
}

void NGContainerFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates,
    const LayoutObject* current_container) {
  DCHECK(descendant_candidates->IsEmpty());

  if (oof_positioned_candidates_.size() == 0)
    return;

  descendant_candidates->ReserveCapacity(oof_positioned_candidates_.size());

  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());
  PhysicalSize builder_physical_size = ToPhysicalSize(Size(), GetWritingMode());

  for (NGOutOfFlowPositionedCandidate& candidate : oof_positioned_candidates_) {
    PhysicalOffset child_offset = candidate.child_offset.ConvertToPhysical(
        GetWritingMode(), Direction(), builder_physical_size, PhysicalSize());

    NGStaticPosition builder_relative_position;
    builder_relative_position.type = candidate.descendant.static_position.type;
    builder_relative_position.offset =
        child_offset + candidate.descendant.static_position.offset;

    // If we are inside the inline algorithm, (and creating a fragment for a
    // <span> or similar), we may add a child (e.g. an atomic-inline) which has
    // OOF descandants.
    //
    // This checks if the object creating this box will be the container for
    // the given descendant.
    const LayoutInline* inline_container =
        candidate.descendant.inline_container;
    if (!inline_container && layout_object_ &&
        layout_object_->IsLayoutInline() &&
        layout_object_->CanContainOutOfFlowPositionedElement(
            candidate.descendant.node.Style().GetPosition()))
      inline_container = ToLayoutInline(layout_object_);

    descendant_candidates->push_back(NGOutOfFlowPositionedDescendant(
        candidate.descendant.node, builder_relative_position,
        inline_container ? ToLayoutInline(inline_container->ContinuationRoot())
                         : nullptr));
    LogicalOffset container_offset =
        builder_relative_position.offset.ConvertToLogical(
            GetWritingMode(), Direction(), builder_physical_size,
            PhysicalSize());
    candidate.descendant.node.SaveStaticOffsetForLegacy(container_offset,
                                                        current_container);
  }

  // Clear our current canidate list. This may get modified again if the
  // current fragment is a containing block, and AddChild is called with a
  // descendant from this list.
  //
  // The descendant may be a "position: absolute" which contains a "position:
  // fixed" for example. (This fragment isn't the containing block for the
  // fixed descendant).
  oof_positioned_candidates_.Shrink(0);
}

#if DCHECK_IS_ON()

String NGContainerFragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.AppendFormat("ContainerFragment %.2fx%.2f, Children %u\n",
                       InlineSize().ToFloat(), BlockSize().ToFloat(),
                       children_.size());
  for (auto& child : children_) {
    builder.Append(child.fragment->DumpFragmentTree(
        NGPhysicalFragment::DumpAll & ~NGPhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

}  // namespace blink
