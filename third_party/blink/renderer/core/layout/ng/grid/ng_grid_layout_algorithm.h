// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT NGGridLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGGridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

  const NGGridBlockTrackCollection& ColumnTrackCollection() const;
  const NGGridBlockTrackCollection& RowTrackCollection() const;

 private:
  struct GridItemData {
    LayoutUnit inline_size;
    MinMaxSizes min_max_sizes;
    NGBoxStrut margins;
  };

 private:
  friend class NGGridLayoutAlgorithmTest;

  void ConstructAndAppendGridItems();
  void ConstructAndAppendGridItem(const NGBlockNode& node);
  GridItemData MeasureGridItem(const NGBlockNode& node);
  NGConstraintSpace BuildSpaceForGridItem(const NGBlockNode& node) const;

  // Sets the specified tracks for row and column track lists.
  void BuildTrackLists();
  // Ensures a range boundary will exist on the start and end of the grid item.
  void EnsureTrackCoverageForGridItem(const NGBlockNode& grid_item);
  // Helper for EnsureTrackCoverageForGridItem.
  static void EnsureTrackCoverageForGridPositions(
      const GridPosition& start_position,
      const GridPosition& end_position,
      NGGridBlockTrackCollection& track_list);

  // Allows a test to set the value for automatic track repetition.
  void SetAutomaticTrackRepetitionsForTesting(wtf_size_t auto_column,
                                              wtf_size_t auto_row);

  // TODO(janewman): Track lists should live on the computed style, mirroring
  // the legacy layout's template_tracks and auto tracks vectors. For now, this
  // method builds a NGGridTrackList from the legacy types that are already
  // computed in style.
  static void AddRepeaters(const Vector<GridTrackSize>& template_tracks,
                           const Vector<GridTrackSize>& auto_tracks,
                           wtf_size_t auto_insertion_point,
                           AutoRepeatType repeat_type,
                           NGGridTrackList& track_list);

  enum class GridLayoutAlgorithmState {
    kMeasuringItems,
  };
  GridLayoutAlgorithmState state_;

  Vector<GridItemData> items_;
  NGGridTrackList column_track_list_;
  NGGridTrackList row_track_list_;
  NGGridBlockTrackCollection column_track_collection_;
  NGGridBlockTrackCollection row_track_collection_;
  wtf_size_t automatic_column_repetitions_for_testing =
      NGGridBlockTrackCollection::kInvalidRangeIndex;
  wtf_size_t automatic_row_repetitions_for_testing =
      NGGridBlockTrackCollection::kInvalidRangeIndex;

  LogicalSize child_percentage_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LAYOUT_ALGORITHM_H_
