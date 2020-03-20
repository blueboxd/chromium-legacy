// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_row_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {
namespace {

inline LayoutUnit InlineOffsetForDisplayMathCentering(
    bool is_display_math,
    LayoutUnit available_inline_size,
    LayoutUnit max_row_inline_size) {
  if (is_display_math)
    return (available_inline_size - max_row_inline_size) / 2;
  return LayoutUnit();
}

}  // namespace

NGMathRowLayoutAlgorithm::NGMathRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding),
      border_scrollbar_padding_(border_padding_ +
                                params.fragment_geometry.scrollbar) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!ConstraintSpace().HasBlockFragmentation());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

void NGMathRowLayoutAlgorithm::LayoutRowItems(
    NGContainerFragmentBuilder::ChildrenVector* children,
    LayoutUnit* max_row_block_baseline,
    LogicalSize* row_total_size) {
  LayoutUnit inline_offset, max_row_ascent, max_row_descent;
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      // TODO(rbuis): OOF should be "where child would have been if not
      // absolutely positioned".
      // Issue: https://github.com/mathml-refresh/mathml/issues/16
      container_builder_.AddOutOfFlowChildCandidate(
          To<NGBlockNode>(child), {border_scrollbar_padding_.inline_start,
                                   border_scrollbar_padding_.block_start});
      continue;
    }
    const ComputedStyle& child_style = child.Style();
    NGConstraintSpace child_space = CreateConstraintSpaceForMathChild(
        Node(), child_available_size_, ConstraintSpace(), child);
    scoped_refptr<const NGLayoutResult> result =
        To<NGBlockNode>(child).Layout(child_space, nullptr /* break token */);
    const NGPhysicalContainerFragment& physical_fragment =
        result->PhysicalFragment();
    NGBoxFragment fragment(ConstraintSpace().GetWritingMode(),
                           ConstraintSpace().Direction(),
                           To<NGPhysicalBoxFragment>(physical_fragment));

    NGBoxStrut margins =
        ComputeMarginsFor(child_space, child_style, ConstraintSpace());
    inline_offset += margins.inline_start;

    LayoutUnit ascent = margins.block_start +
                        fragment.Baseline().value_or(fragment.BlockSize());
    *max_row_block_baseline = std::max(*max_row_block_baseline, ascent);

    // TODO(rbuis): Operators can add lspace and rspace.

    children->emplace_back(
        LogicalOffset{inline_offset, margins.block_start - ascent},
        &physical_fragment);

    inline_offset += fragment.InlineSize() + margins.inline_end;

    max_row_ascent = std::max(max_row_ascent, ascent + margins.block_start);
    max_row_descent = std::max(
        max_row_descent, fragment.BlockSize() + margins.block_end - ascent);
    row_total_size->inline_size =
        std::max(row_total_size->inline_size, inline_offset);
  }
  row_total_size->block_size = max_row_ascent + max_row_descent;
}

scoped_refptr<const NGLayoutResult> NGMathRowLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  bool is_display_math =
      Node().IsMathRoot() && Style().Display() == EDisplay::kMath;

  LogicalSize max_row_size;
  LayoutUnit max_row_block_baseline;

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  child_available_size_ =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);

  NGContainerFragmentBuilder::ChildrenVector children;
  LayoutRowItems(&children, &max_row_block_baseline, &max_row_size);

  // Add children taking into account centering, baseline and
  // border/scrollbar/padding.
  LayoutUnit center_offset = InlineOffsetForDisplayMathCentering(
      is_display_math, container_builder_.InlineSize(),
      max_row_size.inline_size);
  LogicalOffset adjust_offset(
      border_scrollbar_padding_.inline_start + center_offset,
      border_scrollbar_padding_.block_start + max_row_block_baseline);
  for (auto& child : children) {
    child.offset += adjust_offset;
    container_builder_.AddChild(
        To<NGPhysicalContainerFragment>(*child.fragment), child.offset);
  }

  container_builder_.SetBaseline(border_scrollbar_padding_.block_start +
                                 max_row_block_baseline);

  auto block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding_,
      max_row_size.block_size + border_scrollbar_padding_.BlockSum());
  container_builder_.SetBlockSize(block_size);

  NGOutOfFlowLayoutPart(
      Node(), ConstraintSpace(),
      container_builder_.Borders() + container_builder_.Scrollbar(),
      &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSizes> NGMathRowLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  base::Optional<MinMaxSizes> sizes =
      CalculateMinMaxSizesIgnoringChildren(Node(), border_scrollbar_padding_);
  if (sizes)
    return sizes;

  sizes.emplace();
  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          ConstraintSpace(), Node(), border_padding_,
          input.percentage_resolution_block_size);

  MinMaxSizesInput child_input(child_percentage_resolution_block_size);

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    MinMaxSizes child_min_max_sizes =
        ComputeMinAndMaxContentContribution(Style(), child, child_input);
    NGBoxStrut child_margins = ComputeMinMaxMargins(Style(), child);
    child_min_max_sizes += child_margins.InlineSum();
    sizes->max_size += child_min_max_sizes.max_size;
    sizes->min_size += child_min_max_sizes.min_size;

    // TODO(rbuis): Operators can add lspace and rspace.
  }
  sizes->max_size = std::max(sizes->max_size, sizes->min_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes->Encompass(LayoutUnit());
  *sizes += border_scrollbar_padding_.InlineSum();
  return sizes;
}

}  // namespace blink
