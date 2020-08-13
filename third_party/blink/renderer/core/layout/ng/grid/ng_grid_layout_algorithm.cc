// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      state_(GridLayoutAlgorithmState::kMeasuringItems) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!params.break_token);
  container_builder_.SetIsNewFormattingContext(true);

  child_percentage_size_ = CalculateChildPercentageSize(
      ConstraintSpace(), Node(), ChildAvailableSize());
}

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::Layout() {
  // Proceed by algorithm state, as some scenarios will involve a non-linear
  // path through these steps (e.g. skipping or redoing some of them).
  while (state_ != GridLayoutAlgorithmState::kCompletedLayout) {
    switch (state_) {
      case GridLayoutAlgorithmState::kMeasuringItems: {
        SetSpecifiedTracks();
        DetermineExplicitTrackStarts();
        ConstructAndAppendGridItems();
        block_column_track_collection_.FinalizeRanges();
        block_row_track_collection_.FinalizeRanges();

        DCHECK_NE(child_percentage_size_.inline_size, kIndefiniteSize);
        algorithm_column_track_collection_ =
            NGGridLayoutAlgorithmTrackCollection(
                block_column_track_collection_,
                /* is_content_box_size_indefinite */ false);

        bool is_content_box_block_size_indefinite =
            child_percentage_size_.block_size == kIndefiniteSize;
        algorithm_row_track_collection_ = NGGridLayoutAlgorithmTrackCollection(
            block_row_track_collection_, is_content_box_block_size_indefinite);

        state_ = GridLayoutAlgorithmState::kResolvingInlineSize;
        break;
      }

      case GridLayoutAlgorithmState::kResolvingInlineSize:
        ComputeUsedTrackSizes(GridTrackSizingDirection::kForColumns);
        state_ = GridLayoutAlgorithmState::kResolvingBlockSize;
        break;

      case GridLayoutAlgorithmState::kResolvingBlockSize:
        ComputeUsedTrackSizes(GridTrackSizingDirection::kForRows);
        state_ = GridLayoutAlgorithmState::kCompletedLayout;
        break;

      case GridLayoutAlgorithmState::kCompletedLayout:
        NOTREACHED();
        break;
    }
  }
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  return {MinMaxSizes(), /* depends_on_percentage_block_size */ true};
}

const NGGridLayoutAlgorithmTrackCollection&
NGGridLayoutAlgorithm::ColumnTrackCollection() const {
  return algorithm_column_track_collection_;
}

const NGGridLayoutAlgorithmTrackCollection&
NGGridLayoutAlgorithm::RowTrackCollection() const {
  return algorithm_row_track_collection_;
}

NGGridLayoutAlgorithmTrackCollection& NGGridLayoutAlgorithm::TrackCollection(
    GridTrackSizingDirection track_direction) {
  return (track_direction == kForColumns) ? algorithm_column_track_collection_
                                          : algorithm_row_track_collection_;
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItems() {
  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    ConstructAndAppendGridItem(child);
    EnsureTrackCoverageForGridItem(child, kForRows);
    EnsureTrackCoverageForGridItem(child, kForColumns);
  }
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItem(
    const NGBlockNode& node) {
  items_.emplace_back(MeasureGridItem(node));
}

NGGridLayoutAlgorithm::GridItemData NGGridLayoutAlgorithm::MeasureGridItem(
    const NGBlockNode& node) {
  // Before we take track sizing into account for column width contributions,
  // have all child inline and min/max sizes measured for content-based width
  // resolution.
  NGConstraintSpace constraint_space = BuildSpaceForGridItem(node);
  const ComputedStyle& child_style = node.Style();
  bool is_orthogonal_flow_root = !IsParallelWritingMode(
      ConstraintSpace().GetWritingMode(), child_style.GetWritingMode());
  GridItemData grid_item_data;

  // Children with orthogonal writing modes require a full layout pass to
  // determine inline size.
  if (is_orthogonal_flow_root) {
    scoped_refptr<const NGLayoutResult> result = node.Layout(constraint_space);
    grid_item_data.inline_size = NGFragment(ConstraintSpace().GetWritingMode(),
                                            result->PhysicalFragment())
                                     .InlineSize();
  } else {
    NGBoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(constraint_space, node) +
        ComputePadding(constraint_space, child_style);
    grid_item_data.inline_size = ComputeInlineSizeForFragment(
        constraint_space, node, border_padding_in_child_writing_mode);
  }

  grid_item_data.margins =
      ComputeMarginsFor(constraint_space, child_style, ConstraintSpace());

  grid_item_data.min_max_sizes =
      node.ComputeMinMaxSizes(
              ConstraintSpace().GetWritingMode(),
              MinMaxSizesInput(child_percentage_size_.block_size,
                               MinMaxSizesType::kContent),
              &constraint_space)
          .sizes;

  return grid_item_data;
}

NGConstraintSpace NGGridLayoutAlgorithm::BuildSpaceForGridItem(
    const NGBlockNode& node) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         node.Style().GetWritingMode(),
                                         node.CreatesNewFormattingContext());

  space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetTextDirection(node.Style().Direction());
  space_builder.SetIsShrinkToFit(node.Style().LogicalWidth().IsAuto());
  return space_builder.ToConstraintSpace();
}

void NGGridLayoutAlgorithm::SetSpecifiedTracks() {
  const ComputedStyle& grid_style = Style();
  // TODO(kschmi): Auto track repeat count should be based on the number of
  // children, rather than specified auto-column/track.
  // TODO(janewman): We need to implement calculation for track auto repeat
  // count so this can be used outside of testing.
  block_column_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateColumns().NGTrackList(),
      &grid_style.GridAutoColumns().NGTrackList(),
      automatic_column_repetitions_);

  block_row_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateRows().NGTrackList(),
      &grid_style.GridAutoRows().NGTrackList(), automatic_row_repetitions_);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridItem(
    const NGBlockNode& grid_item,
    GridTrackSizingDirection grid_direction) {
  const ComputedStyle& item_style = grid_item.Style();
  GridSpan span = GridPositionsResolver::ResolveGridPositionsFromStyle(
      Style(), item_style, grid_direction,
      AutoRepeatCountForDirection(grid_direction));
  // TODO(janewman): indefinite positions should be resolved with the auto
  // placement algorithm.
  if (span.IsIndefinite())
    return;

  wtf_size_t explicit_start = (grid_direction == kForColumns)
                                  ? explicit_column_start_
                                  : explicit_row_start_;
  wtf_size_t start = span.UntranslatedStartLine() + explicit_start;
  wtf_size_t end = span.UntranslatedEndLine() + explicit_start;

  switch (grid_direction) {
    case kForColumns:
      block_column_track_collection_.EnsureTrackCoverage(start, end - start);
      break;
    case kForRows:
      block_row_track_collection_.EnsureTrackCoverage(start, end - start);
      break;
  }
}

void NGGridLayoutAlgorithm::DetermineExplicitTrackStarts() {
  DCHECK_EQ(0u, explicit_column_start_);
  DCHECK_EQ(0u, explicit_row_start_);

  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    GridSpan column_span = GridPositionsResolver::ResolveGridPositionsFromStyle(
        Style(), child.Style(), kForColumns,
        AutoRepeatCountForDirection(kForColumns));
    GridSpan row_span = GridPositionsResolver::ResolveGridPositionsFromStyle(
        Style(), child.Style(), kForRows,
        AutoRepeatCountForDirection(kForRows));
    if (!column_span.IsIndefinite()) {
      explicit_column_start_ = std::max<int>(
          explicit_column_start_, -column_span.UntranslatedStartLine());
    }
    if (!row_span.IsIndefinite()) {
      explicit_row_start_ =
          std::max<int>(explicit_row_start_, -row_span.UntranslatedStartLine());
    }
  }
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
void NGGridLayoutAlgorithm::ComputeUsedTrackSizes(
    GridTrackSizingDirection track_direction) {
  NGGridLayoutAlgorithmTrackCollection& track_collection =
      TrackCollection(track_direction);
  LayoutUnit content_box_size = (track_direction == kForColumns)
                                    ? child_percentage_size_.inline_size
                                    : child_percentage_size_.block_size;

  // 1. Initialize track sizes (https://drafts.csswg.org/css-grid-1/#algo-init).
  for (auto set_iterator = track_collection.GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    NGGridSet& current_set = set_iterator.CurrentSet();
    const GridTrackSize& track_size = current_set.TrackSize();

    if (track_size.HasFixedMinTrackBreadth()) {
      // Indefinite lengths cannot occur, as they’re treated as 'auto'.
      DCHECK(!track_size.MinTrackBreadth().HasPercentage() ||
             content_box_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial base size.
      LayoutUnit fixed_min_breadth = ValueForLength(
          track_size.MinTrackBreadth().length(), content_box_size);
      current_set.SetBaseSize(fixed_min_breadth * current_set.TrackCount());
    } else {
      // An intrinsic sizing function: Use an initial base size of zero.
      DCHECK(track_size.HasIntrinsicMinTrackBreadth());
      current_set.SetBaseSize(LayoutUnit());
    }

    if (track_size.HasFixedMaxTrackBreadth()) {
      DCHECK(!track_size.MaxTrackBreadth().HasPercentage() ||
             content_box_size != kIndefiniteSize);

      // A fixed sizing function: Resolve to an absolute length and use that
      // size as the track’s initial growth limit; if the growth limit is less
      // than the base size, increase the growth limit to match the base size.
      LayoutUnit fixed_max_breadth = ValueForLength(
          track_size.MaxTrackBreadth().length(), content_box_size);
      current_set.SetGrowthLimit(
          std::max(current_set.BaseSize(),
                   fixed_max_breadth * current_set.TrackCount()));
    } else {
      // An intrinsic or flexible sizing function: Use an initial growth limit
      // of infinity.
      current_set.SetGrowthLimit(kIndefiniteSize);
    }
  }
}

void NGGridLayoutAlgorithm::SetAutomaticTrackRepetitionsForTesting(
    wtf_size_t auto_column,
    wtf_size_t auto_row) {
  automatic_column_repetitions_ = auto_column;
  automatic_row_repetitions_ = auto_row;
}
wtf_size_t NGGridLayoutAlgorithm::AutoRepeatCountForDirection(
    GridTrackSizingDirection direction) const {
  return (direction == kForColumns) ? automatic_column_repetitions_
                                    : automatic_row_repetitions_;
}

}  // namespace blink
