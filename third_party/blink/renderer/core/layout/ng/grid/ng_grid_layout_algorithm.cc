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
      BuildTrackLists();
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

void NGGridLayoutAlgorithm::BuildTrackLists() {
  const ComputedStyle& grid_style = Style();
  AddRepeaters(grid_style.GridTemplateColumns(),
               grid_style.GridAutoRepeatColumns(),
               grid_style.GridAutoRepeatColumnsInsertionPoint(),
               grid_style.GridAutoRepeatColumnsType(), column_track_list_);
  AddRepeaters(grid_style.GridTemplateRows(), grid_style.GridAutoRepeatRows(),
               grid_style.GridAutoRepeatRowsInsertionPoint(),
               grid_style.GridAutoRepeatRowsType(), row_track_list_);

  // TODO(kschmi): Auto track repeat count should be based on the number of
  // children, rather than specified auto-column/track.
  NGGridTrackList implicit_rows;
  NGGridTrackList implicit_columns;
  implicit_rows.AddRepeater(grid_style.GridAutoRows(), /* repeat_count */ 1);
  implicit_columns.AddRepeater(grid_style.GridAutoColumns(),
                               /* repeat_count */ 1);

  // TODO(janewman): We need to implement calculation for track auto repeat
  // count so this can be used outside of testing.
  column_track_collection_.SetSpecifiedTracks(
      column_track_list_, implicit_columns,
      automatic_column_repetitions_for_testing);

  row_track_collection_.SetSpecifiedTracks(
      row_track_list_, implicit_rows, automatic_row_repetitions_for_testing);
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

void NGGridLayoutAlgorithm::AddRepeaters(
    const Vector<GridTrackSize>& template_tracks,
    const Vector<GridTrackSize>& auto_tracks,
    wtf_size_t auto_insertion_point,
    AutoRepeatType repeat_type,
    NGGridTrackList& track_list) {
  wtf_size_t repeat_start = NGGridBlockTrackCollection::kInvalidRangeIndex;
  // TODO(janewman): Track lists should live on the computed style, mirroring
  // the legacy layout's template_tracks and auto tracks vectors. For now, build
  // up the NG version from what already exists on the computed style.
  for (wtf_size_t i = 0; i < template_tracks.size(); ++i) {
    const GridTrackSize& current_track = template_tracks[i];
    // If this is the insertion point for an auto repeater, add it here.
    if (!auto_tracks.IsEmpty() && i == auto_insertion_point) {
      track_list.AddAutoRepeater(auto_tracks, repeat_type);
      repeat_start = NGGridBlockTrackCollection::kInvalidRangeIndex;
    }
    // As the legacy implementation expands repeaters out, compress repeated
    // tracks. As this work will be removed once this is done in style, this is
    // only implemented for the simple case of a single track being repeated,
    // e.g. we will compress repeat(20, 100px) down into a single repeater, but
    // will not compress repeat(20, 10px 20px) at all.
    if (i + 1 < template_tracks.size()) {
      const GridTrackSize& next_track = template_tracks[i + 1];
      if (current_track == next_track && i + 1 != auto_insertion_point) {
        if (repeat_start == NGGridBlockTrackCollection::kInvalidRangeIndex) {
          repeat_start = i;
        }
        continue;
      }
    }
    wtf_size_t repeat_count;
    if (repeat_start == NGGridBlockTrackCollection::kInvalidRangeIndex)
      repeat_count = 1;
    else
      repeat_count = i + 1 - repeat_start;
    DCHECK_NE(0u, repeat_count);
    DCHECK_NE(NGGridBlockTrackCollection::kInvalidRangeIndex, repeat_count);
    track_list.AddRepeater({template_tracks[i]}, repeat_count);
    repeat_start = NGGridBlockTrackCollection::kInvalidRangeIndex;
  }
}

void NGGridLayoutAlgorithm::SetAutomaticTrackRepetitionsForTesting(
    wtf_size_t auto_column,
    wtf_size_t auto_row) {
  automatic_column_repetitions_for_testing = auto_column;
  automatic_row_repetitions_for_testing = auto_row;
}

}  // namespace blink
