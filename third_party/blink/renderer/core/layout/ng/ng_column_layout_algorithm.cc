// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

inline bool NeedsColumnBalancing(LayoutUnit block_size,
                                 const ComputedStyle& style) {
  return block_size == kIndefiniteSize ||
         style.GetColumnFill() == EColumnFill::kBalance;
}

LayoutUnit CalculateColumnContentBlockSize(
    const NGPhysicalContainerFragment& fragment,
    bool multicol_is_horizontal_writing_mode) {
  // TODO(mstensho): Once LayoutNG is capable of calculating overflow on its
  // own, we should probably just move over to relying on that machinery,
  // instead of doing all this on our own.
  LayoutUnit total_size;
  for (const auto& child : fragment.Children()) {
    LayoutUnit size;
    LayoutUnit offset;
    if (multicol_is_horizontal_writing_mode) {
      offset = child.Offset().top;
      size = child->Size().height;
    } else {
      offset = child.Offset().left;
      size = child->Size().width;
    }
    if (child->IsContainer()) {
      LayoutUnit children_size = CalculateColumnContentBlockSize(
          To<NGPhysicalContainerFragment>(*child),
          multicol_is_horizontal_writing_mode);
      if (size < children_size)
        size = children_size;
    }
    LayoutUnit block_end = offset + size;
    if (total_size < block_end)
      total_size = block_end;
  }
  return total_size;
}

}  // namespace

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding),
      border_scrollbar_padding_(border_padding_ +
                                params.fragment_geometry.scrollbar) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

scoped_refptr<const NGLayoutResult> NGColumnLayoutAlgorithm::Layout() {
  LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  LogicalSize content_box_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);
  LogicalSize column_size = CalculateColumnSize(content_box_size);

  WritingMode writing_mode = ConstraintSpace().GetWritingMode();

  // Omit leading border+padding+scrollbar for all fragments but the first.
  LayoutUnit column_block_offset = IsResumingLayout(BreakToken())
                                       ? LayoutUnit()
                                       : border_scrollbar_padding_.block_start;

  bool balance_columns =
      NeedsColumnBalancing(content_box_size.block_size, Style());

  // Figure out how much space we've already been able to process in previous
  // fragments, if this multicol container participates in an outer
  // fragmentation context.
  LayoutUnit previously_consumed_block_size;
  if (const auto* token = BreakToken())
    previously_consumed_block_size = token->ConsumedBlockSize();
  LayoutUnit intrinsic_block_size;

  bool needs_more_fragments_in_outer = false;
  if (ConstraintSpace().HasBlockFragmentation()) {
    // Subtract the space for content we've processed in previous fragments.
    column_size.block_size -= previously_consumed_block_size;

    // Check if we can fit everything (that's remaining), block-wise, within the
    // current outer fragmentainer. If we can't, we need to adjust the block
    // size, and allow the multicol container to continue in a subsequent outer
    // fragmentainer.
    LayoutUnit available_outer_space =
        ConstraintSpace().FragmentainerSpaceAtBfcStart() - column_block_offset;
    if (column_size.block_size > available_outer_space) {
      column_size.block_size = available_outer_space;
      needs_more_fragments_in_outer = true;
    }
  }
  LayoutUnit column_inline_progression =
      column_size.inline_size +
      ResolveUsedColumnGap(content_box_size.inline_size, Style());
  int used_column_count =
      ResolveUsedColumnCount(content_box_size.inline_size, Style());

  if (ConstraintSpace().HasBlockFragmentation())
    container_builder_.SetHasBlockFragmentation();

  do {
    scoped_refptr<const NGBlockBreakToken> break_token;
    if (const auto* token = BreakToken()) {
      // We're resuming layout of this multicol container after an outer
      // fragmentation break. Resume at the break token of the last column that
      // we were able to lay out. Note that in some cases, there may be no child
      // break tokens. That happens if we weren't able to lay out anything at
      // all in the previous outer fragmentainer, e.g. due to a forced break
      // before this multicol container, or e.g. if there was leading
      // unbreakable content that couldn't fit in the space we were offered back
      // then. In other words, in that case, we're about to create the first
      // fragment for this multicol container.
      const auto child_tokens = token->ChildBreakTokens();
      if (child_tokens.size()) {
        break_token =
            To<NGBlockBreakToken>(child_tokens[child_tokens.size() - 1]);
      }
    }

    LayoutUnit column_inline_offset(border_scrollbar_padding_.inline_start);
    int actual_column_count = 0;
    int forced_break_count = 0;

    // Each column should calculate their own minimal space shortage. Find the
    // lowest value of those. This will serve as the column stretch amount, if
    // we determine that stretching them is necessary and possible (column
    // balancing).
    LayoutUnit minimal_space_shortage(LayoutUnit::Max());

    do {
      // This is the first column in this fragmentation context if there are no
      // preceding columns in this row and there are also no preceding rows.
      bool is_first_fragmentainer = !break_token && !BreakToken();

      // Lay out one column. Each column will become a fragment.
      NGConstraintSpace child_space = CreateConstraintSpaceForColumns(
          column_size, is_first_fragmentainer, balance_columns);

      NGFragmentGeometry fragment_geometry =
          CalculateInitialFragmentGeometry(child_space, Node());

      NGBlockLayoutAlgorithm child_algorithm(
          {Node(), fragment_geometry, child_space, break_token.get()});
      child_algorithm.SetBoxType(NGPhysicalFragment::kColumnBox);
      scoped_refptr<const NGLayoutResult> result = child_algorithm.Layout();
      const auto& column = result->PhysicalFragment();

      LogicalOffset logical_offset(column_inline_offset, column_block_offset);
      container_builder_.AddChild(column, logical_offset);

      LayoutUnit space_shortage = result->MinimalSpaceShortage();
      if (space_shortage > LayoutUnit()) {
        minimal_space_shortage =
            std::min(minimal_space_shortage, space_shortage);
      }
      actual_column_count++;
      if (result->HasForcedBreak())
        forced_break_count++;

      LayoutUnit block_size = NGFragment(writing_mode, column).BlockSize();
      intrinsic_block_size =
          std::max(intrinsic_block_size, column_block_offset + block_size);

      column_inline_offset += column_inline_progression;
      break_token = To<NGBlockBreakToken>(column.BreakToken());

      // If we're participating in an outer fragmentation context, we'll only
      // allow as many columns as the used value of column-count, so that we
      // don't overflow in the inline direction. There's one important
      // exception: If we have determined that this is going to be the last
      // fragment for this multicol container in the outer fragmentation
      // context, we'll just allow as many columns as needed (and let them
      // overflow in the inline direction, if necessary). We're not going to
      // progress into a next outer fragmentainer if the (remaining part of the)
      // multicol container fits block-wise in the current outer fragmentainer.
      if (ConstraintSpace().HasBlockFragmentation() && break_token &&
          actual_column_count >= used_column_count &&
          needs_more_fragments_in_outer) {
        container_builder_.SetDidBreak();
        break;
      }
    } while (break_token);

    // TODO(mstensho): Nested column balancing.
    if (container_builder_.DidBreak())
      break;

    // If we overflowed (actual column count larger than what we have room for),
    // and we're supposed to calculate the column lengths automatically (column
    // balancing), see if we're able to stretch them.
    //
    // We can only stretch the columns if we have at least one column that could
    // take more content, and we also need to know the stretch amount (minimal
    // space shortage). We need at least one soft break opportunity to do
    // this. If forced breaks cause too many breaks, there's no stretch amount
    // that could prevent the actual column count from overflowing.
    //
    // TODO(mstensho): Handle this situation also when we're inside another
    // balanced multicol container, rather than bailing (which we do now, to
    // avoid infinite loops). If we exhaust the inner column-count in such
    // cases, that piece of information may have to be propagated to the outer
    // multicol, and instead stretch there (not here). We have no such mechanism
    // in place yet.
    if (actual_column_count > used_column_count &&
        actual_column_count > forced_break_count + 1 &&
        minimal_space_shortage != LayoutUnit::Max() &&
        !ConstraintSpace().IsInsideBalancedColumns()) {
      LayoutUnit new_column_block_size =
          StretchColumnBlockSize(minimal_space_shortage, column_size.block_size,
                                 content_box_size.block_size);

      DCHECK_GE(new_column_block_size, column_size.block_size);
      if (new_column_block_size > column_size.block_size) {
        // Re-attempt layout with taller columns.
        column_size.block_size = new_column_block_size;
        container_builder_.RemoveChildren();
        continue;
      }
    }
    break;
  } while (true);

  // TODO(mstensho): Propagate baselines.

  LayoutUnit block_size;
  if (border_box_size.block_size == kIndefiniteSize) {
    // Get the block size from the columns if it's auto.
    block_size = column_size.block_size + border_scrollbar_padding_.BlockSum();
  } else {
    // TODO(mstensho): end border and padding may overflow the parent
    // fragmentainer, and we should avoid that.
    block_size = border_box_size.block_size - previously_consumed_block_size;
  }

  if (ConstraintSpace().HasBlockFragmentation()) {
    // In addition to establishing one, we're nested inside another
    // fragmentation context.
    FinishFragmentation(&container_builder_, block_size, intrinsic_block_size,
                        previously_consumed_block_size,
                        ConstraintSpace().FragmentainerSpaceAtBfcStart());
  } else {
    container_builder_.SetBlockSize(block_size);
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  }

  NGOutOfFlowLayoutPart(
      Node(), ConstraintSpace(),
      container_builder_.Borders() + container_builder_.Scrollbar(),
      &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGColumnLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  // First calculate the min/max sizes of columns.
  NGConstraintSpace space = CreateConstraintSpaceForMinMax();
  NGFragmentGeometry fragment_geometry =
      CalculateInitialMinMaxFragmentGeometry(space, Node());
  NGBlockLayoutAlgorithm algorithm({Node(), fragment_geometry, space});
  MinMaxSizeInput child_input(input);
  child_input.size_type = NGMinMaxSizeType::kContentBoxSize;
  base::Optional<MinMaxSize> min_max_sizes =
      algorithm.ComputeMinMaxSize(child_input);
  DCHECK(min_max_sizes.has_value());
  MinMaxSize sizes = *min_max_sizes;

  // If column-width is non-auto, pick the larger of that and intrinsic column
  // width.
  if (!Style().HasAutoColumnWidth()) {
    sizes.min_size =
        std::max(sizes.min_size, LayoutUnit(Style().ColumnWidth()));
    sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  }

  // Now convert those column min/max values to multicol container min/max
  // values. We typically have multiple columns and also gaps between them.
  int column_count = Style().ColumnCount();
  DCHECK_GE(column_count, 1);
  sizes.min_size *= column_count;
  sizes.max_size *= column_count;
  LayoutUnit column_gap = ResolveUsedColumnGap(LayoutUnit(), Style());
  sizes += column_gap * (column_count - 1);

  if (input.size_type == NGMinMaxSizeType::kBorderBoxSize) {
    sizes += border_scrollbar_padding_.InlineSum();
  }

  return sizes;
}

LogicalSize NGColumnLayoutAlgorithm::CalculateColumnSize(
    const LogicalSize& content_box_size) {
  LogicalSize column_size = content_box_size;
  DCHECK_GE(column_size.inline_size, LayoutUnit());
  column_size.inline_size =
      ResolveUsedColumnInlineSize(column_size.inline_size, Style());

  if (NeedsColumnBalancing(column_size.block_size, Style())) {
    int used_count =
        ResolveUsedColumnCount(content_box_size.inline_size, Style());
    column_size.block_size =
        CalculateBalancedColumnBlockSize(column_size, used_count);
  }

  return column_size;
}

LayoutUnit NGColumnLayoutAlgorithm::CalculateBalancedColumnBlockSize(
    const LogicalSize& column_size,
    int column_count) {
  // To calculate a balanced column size, we need to figure out how tall our
  // content is. To do that we need to lay out. Create a special constraint
  // space for column balancing, without splitting into fragmentainers. It will
  // make us lay out all the multicol content as one single tall strip. When
  // we're done with this layout pass, we can examine the result and calculate
  // an ideal column block size.
  NGConstraintSpace space = CreateConstraintSpaceForBalancing(column_size);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, Node());
  NGBlockLayoutAlgorithm balancing_algorithm(
      {Node(), fragment_geometry, space});
  scoped_refptr<const NGLayoutResult> result = balancing_algorithm.Layout();

  LayoutUnit single_strip_block_size = CalculateColumnContentBlockSize(
      result->PhysicalFragment(),
      IsHorizontalWritingMode(space.GetWritingMode()));

  // Some extra care is required the division here. We want a the resulting
  // LayoutUnit value to be large enough to prevent overflowing columns. Use
  // floating point to get higher precision than LayoutUnit. Then convert it to
  // a LayoutUnit, but round it up to the nearest value that LayoutUnit is able
  // to represent.
  LayoutUnit block_size = LayoutUnit::FromFloatCeil(
      single_strip_block_size.ToFloat() / static_cast<float>(column_count));

  // Finally, honor {,min-,max-}{height,width} properties.
  return ConstrainColumnBlockSize(block_size);
}

LayoutUnit NGColumnLayoutAlgorithm::StretchColumnBlockSize(
    LayoutUnit minimal_space_shortage,
    LayoutUnit current_column_size,
    LayoutUnit container_content_box_block_size) const {
  if (!NeedsColumnBalancing(container_content_box_block_size, Style()))
    return current_column_size;
  LayoutUnit length = current_column_size + minimal_space_shortage;
  // Honor {,min-,max-}{height,width} properties.
  return ConstrainColumnBlockSize(length);
}

// Constrain a balanced column block size to not overflow the multicol
// container.
LayoutUnit NGColumnLayoutAlgorithm::ConstrainColumnBlockSize(
    LayoutUnit size) const {
  // The {,max-}{height,width} properties are specified on the multicol
  // container, but here we're calculating the column block sizes inside the
  // multicol container, which isn't exactly the same. We may shrink the column
  // block size here, but we'll never stretch it, because the value passed is
  // the perfect balanced block size. Making it taller would only disrupt the
  // balanced output, for no reason. The only thing we need to worry about here
  // is to not overflow the multicol container.

  // First of all we need to convert the size to a value that can be compared
  // against the resolved properties on the multicol container. That means that
  // we have to convert the value from content-box to border-box.
  LayoutUnit extra = border_scrollbar_padding_.BlockSum();
  size += extra;

  const ComputedStyle& style = Style();
  LayoutUnit max = ResolveMaxBlockLength(
      ConstraintSpace(), style, border_padding_, style.LogicalMaxHeight(), size,
      LengthResolvePhase::kLayout);
  LayoutUnit extent = ResolveMainBlockLength(
      ConstraintSpace(), style, border_padding_, style.LogicalHeight(), size,
      LengthResolvePhase::kLayout);
  if (extent != kIndefiniteSize) {
    // A specified height/width will just constrain the maximum length.
    max = std::min(max, extent);
  }

  // Constrain and convert the value back to content-box.
  size = std::min(size, max);
  return size - extra;
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForColumns(
    const LogicalSize& column_size,
    bool is_first_fragmentainer,
    bool balance_columns) const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingMode(), /* is_new_fc */ true);
  space_builder.SetAvailableSize(column_size);
  space_builder.SetPercentageResolutionSize(column_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  // To ensure progression, we need something larger than 0 here. The spec
  // actually says that fragmentainers have to accept at least 1px of content.
  // See https://www.w3.org/TR/css-break-3/#breaking-rules
  LayoutUnit column_block_size =
      std::max(column_size.block_size, LayoutUnit(1));

  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(column_block_size);
  space_builder.SetFragmentainerSpaceAtBfcStart(column_block_size);
  space_builder.SetIsAnonymous(true);
  if (balance_columns)
    space_builder.SetIsInsideBalancedColumns();
  if (!is_first_fragmentainer) {
    // Margins at fragmentainer boundaries should be eaten and truncated to
    // zero. Note that this doesn't apply to margins at forced breaks, but we'll
    // deal with those when we get to them. Set up a margin strut that eats all
    // leading adjacent margins.
    space_builder.SetDiscardingMarginStrut();
  }

  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForBalancing(
    const LogicalSize& column_size) const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingMode(), /* is_new_fc */ true);
  space_builder.SetAvailableSize({column_size.inline_size, kIndefiniteSize});
  space_builder.SetPercentageResolutionSize(column_size);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsIntermediateLayout(true);
  space_builder.SetIsInsideBalancedColumns();

  return space_builder.ToConstraintSpace();
}

NGConstraintSpace NGColumnLayoutAlgorithm::CreateConstraintSpaceForMinMax()
    const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingMode(), /* is_new_fc */ true);
  space_builder.SetIsAnonymous(true);
  space_builder.SetIsIntermediateLayout(true);

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
