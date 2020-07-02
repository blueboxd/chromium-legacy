// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class NGGridLayoutAlgorithmTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGGridForTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGGridLayoutAlgorithmTest()
      : ScopedLayoutNGGridForTest(true),
        ScopedLayoutNGBlockFragmentationForTest(true) {}
  void SetUp() override {
    NGBaseLayoutAlgorithmTest::SetUp();
    style_ = ComputedStyle::Create();
  }

  // Helper methods to access private data on NGGridLayoutAlgorithm. This class
  // is a friend of NGGridLayoutAlgorithm but the individual tests are not.
  size_t GridItemSize(NGGridLayoutAlgorithm& algorithm) {
    return algorithm.items_.size();
  }

  Vector<NGConstraintSpace> GridItemConstraintSpaces(
      NGGridLayoutAlgorithm& algorithm) {
    Vector<NGConstraintSpace> constraint_spaces;
    for (auto& item : algorithm.items_) {
      constraint_spaces.push_back(NGConstraintSpace(item.constraint_space));
    }
    return constraint_spaces;
  }

  void SetAutoTrackRepeat(NGGridLayoutAlgorithm& algorithm,
                          wtf_size_t auto_column,
                          wtf_size_t auto_row) {
    algorithm.SetAutomaticTrackRepetitionsForTesting(auto_column, auto_row);
  }

  scoped_refptr<ComputedStyle> style_;
};

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmMeasuring) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
      grid-template-columns: 100px 100px;
      grid-template-rows: 100px 100px;
    }
    #cell1 {
      grid-column: 1;
      grid-row: 1;
      width: 50px;
    }
    #cell2 {
      grid-column: 2;
      grid-row: 1;
      width: 50px;
    }
    #cell3 {
      grid-column: 1;
      grid-row: 2;
      width: 50px;
    }
    #cell4 {
      grid-column: 2;
      grid-row: 2;
      width: 50px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("grid1")));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(100), LayoutUnit(100)), false, true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemSize(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 5, 5);
  algorithm.Layout();
  EXPECT_EQ(GridItemSize(algorithm), 4U);

  Vector<NGConstraintSpace> constraint_spaces =
      GridItemConstraintSpaces(algorithm);

  EXPECT_EQ(GridItemSize(algorithm), constraint_spaces.size());
  for (auto& constraint_space : constraint_spaces) {
    EXPECT_EQ(constraint_space.AvailableSize().inline_size.ToInt(), 100);
  }
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRanges) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
      grid-template-columns: repeat(2, 100px 100px 200px 200px);
      grid-template-rows: repeat(20, 100px);
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("grid1")));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(100), LayoutUnit(100)), false, true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemSize(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 5, 5);
  algorithm.Layout();
  EXPECT_EQ(GridItemSize(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_EQ(1u, row_iterator.RangeTrackStart());
  EXPECT_EQ(20u, row_iterator.RangeTrackEnd());
  EXPECT_EQ(20u, row_iterator.RepeatCount());
  EXPECT_FALSE(row_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_EQ(1u, column_iterator.RangeTrackStart());
  EXPECT_EQ(2u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(2u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(3u, column_iterator.RangeTrackStart());
  EXPECT_EQ(4u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(2u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(5u, column_iterator.RangeTrackStart());
  EXPECT_EQ(6u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(2u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(7u, column_iterator.RangeTrackStart());
  EXPECT_EQ(8u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(2u, column_iterator.RepeatCount());
  EXPECT_FALSE(column_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesWithAutoRepeater) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
      grid-template-columns: 5px repeat(auto-fit, 150px) repeat(3, 10px);
      grid-template-rows: repeat(20, 100px);
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("grid1")));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(100), LayoutUnit(100)), false, true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemSize(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 5, 5);
  algorithm.Layout();
  EXPECT_EQ(GridItemSize(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_EQ(1u, row_iterator.RangeTrackStart());
  EXPECT_EQ(20u, row_iterator.RangeTrackEnd());
  EXPECT_EQ(20u, row_iterator.RepeatCount());
  EXPECT_FALSE(row_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_EQ(1u, column_iterator.RangeTrackStart());
  EXPECT_EQ(1u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(1u, column_iterator.RepeatCount());
  EXPECT_FALSE(column_iterator.IsRangeCollapsed());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(2u, column_iterator.RangeTrackStart());
  EXPECT_EQ(6u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(5u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.IsRangeCollapsed());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(7u, column_iterator.RangeTrackStart());
  EXPECT_EQ(9u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(3u, column_iterator.RepeatCount());
  EXPECT_FALSE(column_iterator.IsRangeCollapsed());
  EXPECT_FALSE(column_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesImplicit) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
    }
    #cell1 {
      grid-column: 1 / 1;
      grid-row: 1 / 1;
      width: 50px;
    }
    #cell2 {
      grid-column: 2 / 2;
      grid-row: 1 / 1;
      width: 50px;
    }
    #cell3 {
      grid-column: 1 / 1;
      grid-row: 2 / 2;
      width: 50px;
    }
    #cell4 {
      grid-column: 2 / 5;
      grid-row: 2 / 2;
      width: 50px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("grid1")));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(100), LayoutUnit(100)), false, true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemSize(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 5, 5);
  algorithm.Layout();
  EXPECT_EQ(GridItemSize(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_EQ(1u, column_iterator.RangeTrackStart());
  EXPECT_EQ(1u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(1u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(2u, column_iterator.RangeTrackStart());
  EXPECT_EQ(2u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(1u, column_iterator.RepeatCount());
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_EQ(3u, column_iterator.RangeTrackStart());
  EXPECT_EQ(5u, column_iterator.RangeTrackEnd());
  EXPECT_EQ(3u, column_iterator.RepeatCount());
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_EQ(1u, row_iterator.RangeTrackStart());
  EXPECT_EQ(1u, row_iterator.RangeTrackEnd());
  EXPECT_EQ(1u, row_iterator.RepeatCount());
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_EQ(2u, row_iterator.RangeTrackStart());
  EXPECT_EQ(2u, row_iterator.RangeTrackEnd());
  EXPECT_EQ(1u, row_iterator.RepeatCount());
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

}  // namespace blink
