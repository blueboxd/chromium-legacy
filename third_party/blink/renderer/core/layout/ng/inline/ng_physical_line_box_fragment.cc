// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalLineBoxFragment : NGPhysicalContainerFragment {
  NGLineHeightMetrics metrics;
};

static_assert(sizeof(NGPhysicalLineBoxFragment) ==
                  sizeof(SameSizeAsNGPhysicalLineBoxFragment),
              "NGPhysicalLineBoxFragment should stay small");

bool IsInlineLeaf(const NGPhysicalFragment& fragment) {
  if (fragment.IsText())
    return true;
  return fragment.IsBox() && fragment.IsAtomicInline();
}

bool IsEditableFragment(const NGPhysicalFragment& fragment) {
  if (!fragment.GetNode())
    return false;
  return HasEditableStyle(*fragment.GetNode());
}

}  // namespace

scoped_refptr<const NGPhysicalLineBoxFragment>
NGPhysicalLineBoxFragment::Create(NGLineBoxFragmentBuilder* builder) {
  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      sizeof(NGPhysicalLineBoxFragment) +
          builder->children_.size() * sizeof(NGLink),
      ::WTF::GetStringWithTypeName<NGPhysicalLineBoxFragment>());
  new (data) NGPhysicalLineBoxFragment(builder);
  return base::AdoptRef(static_cast<NGPhysicalLineBoxFragment*>(data));
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    NGLineBoxFragmentBuilder* builder)
    : NGPhysicalContainerFragment(builder,
                                  builder->GetWritingMode(),
                                  children_,
                                  kFragmentLineBox,
                                  builder->line_box_type_),
      metrics_(builder->metrics_) {
  // A line box must have a metrics unless it's an empty line box.
  DCHECK(!metrics_.IsEmpty() || IsEmptyLineBox());
  base_direction_ = static_cast<unsigned>(builder->base_direction_);
  has_hanging_ = builder->hang_inline_size_ != 0;
  has_propagated_descendants_ = has_floating_descendants_ ||
                                HasOutOfFlowPositionedDescendants() ||
                                builder->unpositioned_list_marker_;
}

NGLineHeightMetrics NGPhysicalLineBoxFragment::BaselineMetrics(
    FontBaseline) const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflow(
    const LayoutObject* container,
    const ComputedStyle* container_style,
    PhysicalSize container_physical_size) const {
  WritingMode container_writing_mode = container_style->GetWritingMode();
  TextDirection container_direction = container_style->Direction();
  PhysicalRect overflow({}, Size());
  for (const auto& child : Children()) {
    PhysicalRect child_scroll_overflow =
        child->ScrollableOverflowForPropagation(container);
    child_scroll_overflow.offset += child.Offset();

    // Chop the hanging part from scrollable overflow. Children overflow in
    // inline direction should hang, which should not cause scroll.
    // TODO(kojii): Should move to text fragment to make this more accurate.
    if (UNLIKELY(has_hanging_ && !child->IsFloatingOrOutOfFlowPositioned())) {
      if (IsHorizontalWritingMode(container_writing_mode)) {
        if (child_scroll_overflow.offset.left < 0)
          child_scroll_overflow.offset.left = LayoutUnit();
        if (child_scroll_overflow.Right() > Size().width)
          child_scroll_overflow.ShiftRightEdgeTo(Size().width);
      } else {
        if (child_scroll_overflow.offset.top < 0)
          child_scroll_overflow.offset.top = LayoutUnit();
        if (child_scroll_overflow.Bottom() > Size().height)
          child_scroll_overflow.ShiftBottomEdgeTo(Size().height);
      }
    }

    // If child has the same style as parent, parent will compute relative
    // offset.
    if (&child->Style() != container_style) {
      child_scroll_overflow.offset +=
          ComputeRelativeOffset(child->Style(), container_writing_mode,
                                container_direction, container_physical_size);
    }
    overflow.Unite(child_scroll_overflow);
  }
  return overflow;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::FirstLogicalLeaf() const {
  if (Children().empty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (const auto* runner_as_container =
             DynamicTo<NGPhysicalContainerFragment>(runner)) {
    if (runner->IsBlockFormattingContextRoot())
      break;
    if (runner_as_container->Children().empty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().front().get()
                 : runner_as_container->Children().back().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::LastLogicalLeaf() const {
  if (Children().empty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (const auto* runner_as_container =
             DynamicTo<NGPhysicalContainerFragment>(runner)) {
    if (runner->IsBlockFormattingContextRoot())
      break;
    if (runner_as_container->Children().empty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().back().get()
                 : runner_as_container->Children().front().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  const auto& break_token = To<NGInlineBreakToken>(*BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

PhysicalOffset NGPhysicalLineBoxFragment::LineStartPoint() const {
  const LogicalOffset logical_start;  // (0, 0)
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_start.ConvertToPhysical(Style().GetWritingMode(),
                                         BaseDirection(), Size(), pixel_size);
}

PhysicalOffset NGPhysicalLineBoxFragment::LineEndPoint() const {
  const LayoutUnit inline_size =
      NGFragment(Style().GetWritingMode(), *this).InlineSize();
  const LogicalOffset logical_end(inline_size, LayoutUnit());
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_end.ConvertToPhysical(Style().GetWritingMode(),
                                       BaseDirection(), Size(), pixel_size);
}

const LayoutObject* NGPhysicalLineBoxFragment::ClosestLeafChildForPoint(
    const PhysicalOffset& point,
    bool only_editable_leaves) const {
  const PhysicalSize unit_square(LayoutUnit(1), LayoutUnit(1));
  const LogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), BaseDirection(), Size(), unit_square);
  const LayoutUnit inline_offset = logical_point.inline_offset;
  const NGPhysicalFragment* closest_leaf_child = nullptr;
  LayoutUnit closest_leaf_distance;
  for (const auto& descendant :
       NGInlineFragmentTraversal::DescendantsOf(*this)) {
    const NGPhysicalFragment& fragment = *descendant.fragment;
    if (!fragment.GetLayoutObject())
      continue;
    if (!IsInlineLeaf(fragment) || fragment.IsListMarker())
      continue;
    if (only_editable_leaves && !IsEditableFragment(fragment))
      continue;

    const LogicalSize fragment_logical_size =
        fragment.Size().ConvertToLogical(Style().GetWritingMode());
    const LogicalOffset fragment_logical_offset =
        descendant.offset_to_container_box.ConvertToLogical(
            Style().GetWritingMode(), BaseDirection(), Size(), fragment.Size());
    const LayoutUnit inline_min = fragment_logical_offset.inline_offset;
    const LayoutUnit inline_max = fragment_logical_offset.inline_offset +
                                  fragment_logical_size.inline_size;
    if (inline_offset >= inline_min && inline_offset < inline_max)
      return fragment.GetLayoutObject();

    const LayoutUnit distance =
        inline_offset < inline_min ? inline_min - inline_offset
                                   : inline_offset - inline_max + LayoutUnit(1);
    if (!closest_leaf_child || distance < closest_leaf_distance) {
      closest_leaf_child = &fragment;
      closest_leaf_distance = distance;
    }
  }
  if (!closest_leaf_child)
    return nullptr;
  return closest_leaf_child->GetLayoutObject();
}

}  // namespace blink
