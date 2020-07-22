// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

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
  switch (state_) {
    case GridLayoutAlgorithmState::kMeasuringItems:
      SetSpecifiedTracks();
      ConstructAndAppendGridItems();
      row_track_collection_.FinalizeRanges();
      column_track_collection_.FinalizeRanges();
      break;

    default:
      break;
  }

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  return {MinMaxSizes(), /* depends_on_percentage_block_size */ true};
}

const NGGridBlockTrackCollection& NGGridLayoutAlgorithm::ColumnTrackCollection()
    const {
  return column_track_collection_;
}

const NGGridBlockTrackCollection& NGGridLayoutAlgorithm::RowTrackCollection()
    const {
  return row_track_collection_;
}

void NGGridLayoutAlgorithm::ConstructAndAppendGridItems() {
  NGGridChildIterator iterator(Node());
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    ConstructAndAppendGridItem(child);
    EnsureTrackCoverageForGridItem(child);
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
        ComputeBorders(constraint_space, child_style) +
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
  column_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateColumns().NGTrackList(),
      &grid_style.GridAutoColumns().NGTrackList(),
      automatic_column_repetitions_for_testing);

  row_track_collection_.SetSpecifiedTracks(
      &grid_style.GridTemplateRows().NGTrackList(),
      &grid_style.GridAutoRows().NGTrackList(),
      automatic_row_repetitions_for_testing);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridItem(
    const NGBlockNode& grid_item) {
  const ComputedStyle& item_style = grid_item.Style();
  EnsureTrackCoverageForGridPositions(item_style.GridColumnStart(),
                                      item_style.GridColumnEnd(),
                                      column_track_collection_);
  EnsureTrackCoverageForGridPositions(item_style.GridRowStart(),
                                      item_style.GridRowEnd(),
                                      row_track_collection_);
}

void NGGridLayoutAlgorithm::EnsureTrackCoverageForGridPositions(
    const GridPosition& start_position,
    const GridPosition& end_position,
    NGGridBlockTrackCollection& track_collection) {
  // For now, we only support adding tracks if they were specified.
  // TODO(janewman): Implement support for position types other than Explicit.
  if (start_position.GetType() == GridPositionType::kExplicitPosition &&
      end_position.GetType() == GridPositionType::kExplicitPosition) {
    track_collection.EnsureTrackCoverage(
        start_position.IntegerPosition(),
        end_position.IntegerPosition() - start_position.IntegerPosition() + 1);
  }
}

void NGGridLayoutAlgorithm::SetAutomaticTrackRepetitionsForTesting(
    wtf_size_t auto_column,
    wtf_size_t auto_row) {
  automatic_column_repetitions_for_testing = auto_column;
  automatic_row_repetitions_for_testing = auto_row;
}

}  // namespace blink
