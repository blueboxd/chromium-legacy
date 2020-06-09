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
  algorithm.Layout();
  EXPECT_EQ(GridItemSize(algorithm), 4U);

  Vector<NGConstraintSpace> constraint_spaces =
      GridItemConstraintSpaces(algorithm);

  EXPECT_EQ(GridItemSize(algorithm), constraint_spaces.size());
  for (auto& constraint_space : constraint_spaces) {
    EXPECT_EQ(constraint_space.AvailableSize().inline_size.ToInt(), 100);
  }
}

}  // namespace blink
