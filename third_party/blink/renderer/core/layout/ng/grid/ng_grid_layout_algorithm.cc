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
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);

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
  GridItem item;
  item.constraint_space = BuildSpaceForMeasure(node);
  items_.emplace_back(item);
}

NGConstraintSpace NGGridLayoutAlgorithm::BuildSpaceForMeasure(
    const NGBlockNode& grid_item) {
  const ComputedStyle& child_style = grid_item.Style();

  NGConstraintSpaceBuilder space_builder(ConstraintSpace(),
                                         child_style.GetWritingMode(),
                                         /* is_new_fc */ true);
  space_builder.SetCacheSlot(NGCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);

  // TODO(kschmi): - do layout/measuring and handle non-fixed sizes here.
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetTextDirection(child_style.Direction());
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
  NGGridTrackList implicit_columns;
  NGGridTrackList implicit_rows;
  implicit_columns.AddRepeater(
      /*track_index*/ 0, /*track_count*/ 1,
      /*repeat_count*/ grid_style.GridAutoColumns().size());
  implicit_rows.AddRepeater(/*track_index*/ 0, 1 /*track_count*/,
                            /*repeat_count*/ grid_style.GridAutoRows().size());

  // TODO(janewman): We need to implement calculation for track auto repeat
  // count so this can be used outside of testing.
  column_track_collection_.SetSpecifiedTracks(
      column_track_list_, automatic_column_repetitions_for_testing,
      implicit_columns);

  row_track_collection_.SetSpecifiedTracks(
      row_track_list_, automatic_row_repetitions_for_testing, implicit_rows);
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
  wtf_size_t unique_track_count = 0;
  // TODO(janewman): Track lists should live on the computed style, mirroring
  // the legacy layout's template_tracks and auto tracks vectors. For now, build
  // up the NG version from what already exists on the computed style.
  for (wtf_size_t i = 0; i < template_tracks.size(); ++i) {
    const GridTrackSize& current_track = template_tracks[i];
    // If this is the insertion point for an auto repeater, add it here.
    if (!auto_tracks.IsEmpty() && i == auto_insertion_point) {
      track_list.AddAutoRepeater(unique_track_count, auto_tracks.size(),
                                 repeat_type);
      unique_track_count += auto_tracks.size();
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
    track_list.AddRepeater(unique_track_count++, /*track_count*/ 1,
                           repeat_count);
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
