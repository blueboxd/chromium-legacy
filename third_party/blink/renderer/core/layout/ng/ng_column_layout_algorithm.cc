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
  content_box_size_ =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);

  DCHECK_GE(content_box_size_.inline_size, LayoutUnit());
  column_inline_size_ =
      ResolveUsedColumnInlineSize(content_box_size_.inline_size, Style());

  column_inline_progression_ =
      column_inline_size_ +
      ResolveUsedColumnGap(content_box_size_.inline_size, Style());
  used_column_count_ =
      ResolveUsedColumnCount(content_box_size_.inline_size, Style());

  if (ConstraintSpace().HasBlockFragmentation())
    container_builder_.SetHasBlockFragmentation();
  container_builder_.SetIsBlockFragmentationContextRoot();

  // Omit leading border+padding+scrollbar for all fragments but the first.
  if (!IsResumingLayout(BreakToken()))
    intrinsic_block_size_ = border_scrollbar_padding_.block_start;

  scoped_refptr<const NGBreakToken> child_break_token;
  if (const auto* token = BreakToken()) {
    // We're resuming layout of this multicol container after an outer
    // fragmentation break. Resume at the break token of the last column that we
    // were able to lay out. Note that in some cases, there may be no child
    // break tokens. That happens if we weren't able to lay out anything at all
    // in the previous outer fragmentainer, e.g. due to a forced break before
    // this multicol container, or e.g. if there was leading unbreakable content
    // that couldn't fit in the space we were offered back then. In other words,
    // in that case, we're about to create the first fragment for this multicol
    // container.
    const auto child_tokens = token->ChildBreakTokens();
    if (child_tokens.size()) {
      DCHECK_EQ(child_tokens.size(), 1u);
      child_break_token = child_tokens[0];
    }
  }

  if (!BreakToken() || !BreakToken()->HasSeenAllChildren()) {
    child_break_token =
        LayoutRow(To<NGBlockBreakToken>(child_break_token.get()));
  }

  if (!child_break_token) {
    // We've gone through all the content. This doesn't necessarily mean that
    // we're done fragmenting, since the multicol container may be taller than
    // what the content requires, which means that we might create more
    // (childless) fragments, if we're nested inside another fragmentation
    // context. In that case we must make sure to skip the contents when
    // resuming.
    container_builder_.SetHasSeenAllChildren();
  }

  // Figure out how much space we've already been able to process in previous
  // fragments, if this multicol container participates in an outer
  // fragmentation context.
  LayoutUnit previously_consumed_block_size;
  if (const auto* token = BreakToken())
    previously_consumed_block_size = token->ConsumedBlockSize();

  // TODO(mstensho): Propagate baselines.

  LayoutUnit block_size;
  if (border_box_size.block_size == kIndefiniteSize) {
    // Get the block size from the contents if it's auto.
    block_size = intrinsic_block_size_ + border_scrollbar_padding_.block_end;
  } else {
    // TODO(mstensho): end border and padding may overflow the parent
    // fragmentainer, and we should avoid that.
    block_size = border_box_size.block_size - previously_consumed_block_size;
  }

  if (ConstraintSpace().HasBlockFragmentation()) {
    if (child_break_token)
      container_builder_.AddBreakToken(std::move(child_break_token));

    // In addition to establishing one, we're nested inside another
    // fragmentation context.
    FinishFragmentation(&container_builder_, block_size, intrinsic_block_size_,
                        previously_consumed_block_size,
                        ConstraintSpace().FragmentainerSpaceAtBfcStart());
  } else {
    container_builder_.SetBlockSize(block_size);
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
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

scoped_refptr<const NGBreakToken> NGColumnLayoutAlgorithm::LayoutRow(
    const NGBlockBreakToken* next_column_token) {
  LayoutUnit column_block_offset = intrinsic_block_size_;
  LogicalSize column_size(column_inline_size_, content_box_size_.block_size);

  // If block-size is non-auto, subtract the space for content we've consumed in
  // previous fragments. This is necessary when we're nested inside another
  // fragmentation context.
  if (ConstraintSpace().HasBlockFragmentation() &&
      column_size.block_size != kIndefiniteSize) {
    if (const auto* token = BreakToken()) {
      column_size.block_size -= token->ConsumedBlockSize();
      column_size.block_size = column_size.block_size.ClampNegativeToZero();
    }
  }

  // We balance if block-size is unconstrained, or when we're explicitly told
  // to. Note that the block-size may be constrained by outer fragmentation
  // contexts, not just by a block-size specified on this multicol container.
  bool balance_columns = Style().GetColumnFill() == EColumnFill::kBalance ||
                         (column_size.block_size == kIndefiniteSize &&
                          !ConstraintSpace().HasBlockFragmentation());

  if (balance_columns) {
    column_size.block_size =
        CalculateBalancedColumnBlockSize(column_size, next_column_token);
  }

  bool needs_more_fragments_in_outer = false;
  if (ConstraintSpace().HasBlockFragmentation()) {
    LayoutUnit available_outer_space =
        ConstraintSpace().FragmentainerSpaceAtBfcStart() - column_block_offset;

    // TODO(mstensho): This should never be negative, or even zero. Turn into a
    // DCHECK when the underlying problem is fixed.
    available_outer_space = available_outer_space.ClampNegativeToZero();

    // Check if we can fit everything (that's remaining), block-wise, within the
    // current outer fragmentainer. If we can't, we need to adjust the block
    // size, and allow the multicol container to continue in a subsequent outer
    // fragmentainer. Note that we also need to handle indefinite block-size,
    // because that may happen in a nested multicol container with auto
    // block-size and column balancing disabled.
    if (column_size.block_size > available_outer_space ||
        column_size.block_size == kIndefiniteSize) {
      column_size.block_size = available_outer_space;
      needs_more_fragments_in_outer = true;
    }
  }

  DCHECK_GE(column_size.block_size, LayoutUnit());

  // New column fragments won't be added to the fragment builder right away,
  // since we may need to delete them and try again with a different block-size
  // (colum balancing). Keep them in this list, and add them to the fragment
  // builder when we have the final column fragments. Or clear the list and
  // retry otherwise.
  NGContainerFragmentBuilder::ChildrenVector new_columns;

  scoped_refptr<const NGBreakToken> last_break_token;

  do {
    scoped_refptr<const NGBlockBreakToken> column_break_token =
        next_column_token;

    // This is the first column in this fragmentation context if there are no
    // preceding columns in this row and there are also no preceding rows.
    bool is_first_fragmentainer = !column_break_token && !BreakToken();

    LayoutUnit column_inline_offset(border_scrollbar_padding_.inline_start);
    int actual_column_count = 0;
    int forced_break_count = 0;

    // Each column should calculate their own minimal space shortage. Find the
    // lowest value of those. This will serve as the column stretch amount, if
    // we determine that stretching them is necessary and possible (column
    // balancing).
    LayoutUnit minimal_space_shortage(LayoutUnit::Max());

    do {
      // Lay out one column. Each column will become a fragment.
      NGConstraintSpace child_space = CreateConstraintSpaceForColumns(
          column_size, is_first_fragmentainer, balance_columns);

      NGFragmentGeometry fragment_geometry =
          CalculateInitialFragmentGeometry(child_space, Node());

      NGBlockLayoutAlgorithm child_algorithm(
          {Node(), fragment_geometry, child_space, column_break_token.get()});
      child_algorithm.SetBoxType(NGPhysicalFragment::kColumnBox);
      scoped_refptr<const NGLayoutResult> result = child_algorithm.Layout();
      const auto& column = result->PhysicalFragment();

      // Add the new column fragment to the list, but don't commit anything to
      // the fragment builder until we know whether these are the final columns.
      LogicalOffset logical_offset(column_inline_offset, column_block_offset);
      new_columns.emplace_back(logical_offset, &result->PhysicalFragment());

      LayoutUnit space_shortage = result->MinimalSpaceShortage();
      if (space_shortage > LayoutUnit()) {
        minimal_space_shortage =
            std::min(minimal_space_shortage, space_shortage);
      }
      actual_column_count++;
      if (result->HasForcedBreak())
        forced_break_count++;

      column_inline_offset += column_inline_progression_;

      last_break_token = column.BreakToken();
      column_break_token = To<NGBlockBreakToken>(last_break_token.get());

      // If we're participating in an outer fragmentation context, we'll only
      // allow as many columns as the used value of column-count, so that we
      // don't overflow in the inline direction. There's one important
      // exception: If we have determined that this is going to be the last
      // fragment for this multicol container in the outer fragmentation
      // context, we'll just allow as many columns as needed (and let them
      // overflow in the inline direction, if necessary). We're not going to
      // progress into a next outer fragmentainer if the (remaining part of the)
      // multicol container fits block-wise in the current outer fragmentainer.
      if (ConstraintSpace().HasBlockFragmentation() && column_break_token &&
          actual_column_count >= used_column_count_ &&
          needs_more_fragments_in_outer) {
        container_builder_.SetDidBreak();
        break;
      }

      is_first_fragmentainer = false;
    } while (column_break_token);

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
    if (balance_columns && actual_column_count > used_column_count_ &&
        actual_column_count > forced_break_count + 1 &&
        minimal_space_shortage != LayoutUnit::Max() &&
        !ConstraintSpace().IsInsideBalancedColumns()) {
      LayoutUnit new_column_block_size = StretchColumnBlockSize(
          minimal_space_shortage, column_size.block_size);

      DCHECK_GE(new_column_block_size, column_size.block_size);
      if (new_column_block_size > column_size.block_size) {
        // Remove column fragments and re-attempt layout with taller columns.
        new_columns.clear();
        column_size.block_size = new_column_block_size;
        continue;
      }
    }
    break;
  } while (true);

  // If there was no content inside to process, we don't want the resulting
  // empty column fragment.
  if (new_columns.size() == 1u) {
    const NGPhysicalBoxFragment& column =
        *To<NGPhysicalBoxFragment>(new_columns[0].fragment.get());
    // TODO(mstensho): Keeping the empty fragment, just so that out-of-flow
    // descendants get propagated correctly isn't right. Find some other way of
    // propagating them.
    if (column.Children().size() == 0 &&
        !column.HasOutOfFlowPositionedDescendants())
      return last_break_token;
  }

  intrinsic_block_size_ += column_size.block_size;

  // Commit all column fragments to the fragment builder.
  for (auto column : new_columns) {
    container_builder_.AddChild(To<NGPhysicalBoxFragment>(*column.fragment),
                                column.offset);
  }

  return last_break_token;
}

LayoutUnit NGColumnLayoutAlgorithm::CalculateBalancedColumnBlockSize(
    const LogicalSize& column_size,
    const NGBlockBreakToken* child_break_token) {
  // To calculate a balanced column size for one row of columns, we need to
  // figure out how tall our content is. To do that we need to lay out. Create a
  // special constraint space for column balancing, without allowing soft
  // breaks. It will make us lay out all the multicol content as one single tall
  // strip (unless there are forced breaks). When we're done with this layout
  // pass, we can examine the result and calculate an ideal column block-size.
  NGConstraintSpace space = CreateConstraintSpaceForBalancing(column_size);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, Node());

  // A run of content without explicit (forced) breaks; i.e. the content portion
  // between two explicit breaks, between fragmentation context start and an
  // explicit break, between an explicit break and fragmentation context end,
  // or, in cases when there are no explicit breaks at all: between
  // fragmentation context start and end. We need to know where the explicit
  // breaks are, in order to figure out where the implicit breaks will end up,
  // so that we get the columns properly balanced. A content run starts out as
  // representing one single column, and we'll add as many additional implicit
  // breaks as needed into the content runs that are the tallest ones
  // (ColumnBlockSize()).
  struct ContentRun {
    ContentRun(LayoutUnit content_block_size)
        : content_block_size(content_block_size) {}

    // Return the column block-size that this content run would require,
    // considering the implicit breaks we have assumed so far.
    LayoutUnit ColumnBlockSize() const {
      // Some extra care is required for the division here. We want the
      // resulting LayoutUnit value to be large enough to prevent overflowing
      // columns. Use floating point to get higher precision than
      // LayoutUnit. Then convert it to a LayoutUnit, but round it up to the
      // nearest value that LayoutUnit is able to represent.
      return LayoutUnit::FromFloatCeil(
          float(content_block_size) / float(implicit_breaks_assumed_count + 1));
    }

    LayoutUnit content_block_size;

    // The number of implicit breaks assumed to exist in this content run.
    int implicit_breaks_assumed_count = 0;
  };

  class ContentRuns : public Vector<ContentRun, 1> {
   public:
    wtf_size_t IndexWithTallestColumns() const {
      DCHECK_GT(size(), 0u);
      wtf_size_t index = 0;
      LayoutUnit largest_block_size = LayoutUnit::Min();
      for (size_t i = 0; i < size(); i++) {
        const ContentRun& run = at(i);
        LayoutUnit block_size = run.ColumnBlockSize();
        if (largest_block_size < block_size) {
          largest_block_size = block_size;
          index = i;
        }
      }
      return index;
    }

    // When we have "inserted" (assumed) enough implicit column breaks, this
    // method returns the block-size of the tallest column.
    LayoutUnit TallestColumnBlockSize() const {
      return at(IndexWithTallestColumns()).ColumnBlockSize();
    }
  };

  // First split into content runs at explicit (forced) breaks.
  ContentRuns content_runs;
  scoped_refptr<const NGBlockBreakToken> break_token = child_break_token;
  do {
    NGBlockLayoutAlgorithm balancing_algorithm(
        {Node(), fragment_geometry, space, break_token.get()});
    scoped_refptr<const NGLayoutResult> result = balancing_algorithm.Layout();
    const NGPhysicalBoxFragment& fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    break_token = To<NGBlockBreakToken>(fragment.BreakToken());
    LayoutUnit column_block_size = CalculateColumnContentBlockSize(
        fragment, IsHorizontalWritingMode(space.GetWritingMode()));
    content_runs.emplace_back(column_block_size);
  } while (break_token);

  // Then distribute as many implicit breaks into the content runs as we need.
  int used_column_count =
      ResolveUsedColumnCount(content_box_size_.inline_size, Style());
  for (int columns_found = content_runs.size();
       columns_found < used_column_count; columns_found++) {
    // The tallest content run (with all assumed implicit breaks added so far
    // taken into account) is where we assume the next implicit break.
    wtf_size_t index = content_runs.IndexWithTallestColumns();
    content_runs[index].implicit_breaks_assumed_count++;
  }

  // We now have an estimated minimal block-size for the columns. Roughly
  // speaking, this is the block-size that the columns will need if we are
  // allowed to break freely at any offset. This is normally not the case,
  // though, since there will typically be unbreakable pieces of content, such
  // as replaced content, lines of text, and other things. We need to actually
  // lay out into columns to figure out if they are tall enough or not (and
  // stretch and retry if not). Also honor {,min-,max-}{height,width} properties
  // before returning.
  LayoutUnit block_size = content_runs.TallestColumnBlockSize();
  return ConstrainColumnBlockSize(block_size);
}

LayoutUnit NGColumnLayoutAlgorithm::StretchColumnBlockSize(
    LayoutUnit minimal_space_shortage,
    LayoutUnit current_column_size) const {
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
  space_builder.SetFragmentationType(kFragmentColumn);
  // TODO(mstensho): It would be better to use kIndefiniteSize here, rather than
  // LayoutUnit::Max(), but the fragmentation machinery currently gets confused
  // by such a value.
  space_builder.SetFragmentainerBlockSize(LayoutUnit::Max());
  space_builder.SetFragmentainerSpaceAtBfcStart(LayoutUnit::Max());
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
