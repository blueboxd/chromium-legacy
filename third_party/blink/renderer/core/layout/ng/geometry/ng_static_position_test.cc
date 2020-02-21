// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using InlineEdge = NGLogicalStaticPosition::InlineEdge;
using BlockEdge = NGLogicalStaticPosition::BlockEdge;
using HorizontalEdge = NGPhysicalStaticPosition::HorizontalEdge;
using VerticalEdge = NGPhysicalStaticPosition::VerticalEdge;

struct NGStaticPositionTestData {
  NGLogicalStaticPosition logical;
  NGPhysicalStaticPosition physical;
  WritingMode writing_mode;
  base::i18n::TextDirection direction;

} ng_static_position_test_data[] = {
    // |WritingMode::kHorizontalTb|, |base::i18n::TextDirection::LEFT_TO_RIGHT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(20, 30), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    // |WritingMode::kHorizontalTb|, |base::i18n::TextDirection::RIGHT_TO_LEFT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(80, 30), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kHorizontalTb,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    // |WritingMode::kVerticalRl|, |base::i18n::TextDirection::LEFT_TO_RIGHT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(70, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    // |WritingMode::kVerticalRl|, |base::i18n::TextDirection::RIGHT_TO_LEFT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(70, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    // |WritingMode::kVerticalLr|, |base::i18n::TextDirection::LEFT_TO_RIGHT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    // |WritingMode::kVerticalLr|, |base::i18n::TextDirection::RIGHT_TO_LEFT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    // |WritingMode::kSidewaysLr|, |base::i18n::TextDirection::LEFT_TO_RIGHT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::LEFT_TO_RIGHT},
    // |WritingMode::kSidewaysLr|, |base::i18n::TextDirection::RIGHT_TO_LEFT|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     base::i18n::TextDirection::RIGHT_TO_LEFT},
};

class NGStaticPositionTest
    : public testing::Test,
      public testing::WithParamInterface<NGStaticPositionTestData> {};

TEST_P(NGStaticPositionTest, Convert) {
  const auto& data = GetParam();

  // These tests take the logical static-position, and convert it to a physical
  // static-position with a 100x100 rect.
  //
  // It asserts that it is the same as the expected physical static-position,
  // then performs the same operation in reverse.

  NGPhysicalStaticPosition physical_result = data.logical.ConvertToPhysical(
      data.writing_mode, data.direction, PhysicalSize(100, 100));
  EXPECT_EQ(physical_result.offset, data.physical.offset);
  EXPECT_EQ(physical_result.horizontal_edge, data.physical.horizontal_edge);
  EXPECT_EQ(physical_result.vertical_edge, data.physical.vertical_edge);

  NGLogicalStaticPosition logical_result = data.physical.ConvertToLogical(
      data.writing_mode, data.direction, PhysicalSize(100, 100));
  EXPECT_EQ(logical_result.offset, data.logical.offset);
  EXPECT_EQ(logical_result.inline_edge, data.logical.inline_edge);
  EXPECT_EQ(logical_result.block_edge, data.logical.block_edge);
}

INSTANTIATE_TEST_SUITE_P(NGStaticPositionTest,
                         NGStaticPositionTest,
                         testing::ValuesIn(ng_static_position_test_data));

}  // namespace
}  // namespace blink
