// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

TEST(GeometryUnitsTest, ConvertLogicalOffsetToPhysicalOffset) {
  LogicalOffset logical_offset(20, 30);
  PhysicalSize outer_size(300, 400);
  PhysicalSize inner_size(5, 65);
  PhysicalOffset offset;

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kHorizontalTb, base::i18n::TextDirection::LEFT_TO_RIGHT,
      outer_size, inner_size);
  EXPECT_EQ(20, offset.left);
  EXPECT_EQ(30, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kHorizontalTb, base::i18n::TextDirection::RIGHT_TO_LEFT,
      outer_size, inner_size);
  EXPECT_EQ(275, offset.left);
  EXPECT_EQ(30, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kVerticalRl, base::i18n::TextDirection::LEFT_TO_RIGHT,
      outer_size, inner_size);
  EXPECT_EQ(265, offset.left);
  EXPECT_EQ(20, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kVerticalRl, base::i18n::TextDirection::RIGHT_TO_LEFT,
      outer_size, inner_size);
  EXPECT_EQ(265, offset.left);
  EXPECT_EQ(315, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kSidewaysRl, base::i18n::TextDirection::LEFT_TO_RIGHT,
      outer_size, inner_size);
  EXPECT_EQ(265, offset.left);
  EXPECT_EQ(20, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kSidewaysRl, base::i18n::TextDirection::RIGHT_TO_LEFT,
      outer_size, inner_size);
  EXPECT_EQ(265, offset.left);
  EXPECT_EQ(315, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kVerticalLr, base::i18n::TextDirection::LEFT_TO_RIGHT,
      outer_size, inner_size);
  EXPECT_EQ(30, offset.left);
  EXPECT_EQ(20, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kVerticalLr, base::i18n::TextDirection::RIGHT_TO_LEFT,
      outer_size, inner_size);
  EXPECT_EQ(30, offset.left);
  EXPECT_EQ(315, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kSidewaysLr, base::i18n::TextDirection::LEFT_TO_RIGHT,
      outer_size, inner_size);
  EXPECT_EQ(30, offset.left);
  EXPECT_EQ(315, offset.top);

  offset = logical_offset.ConvertToPhysical(
      WritingMode::kSidewaysLr, base::i18n::TextDirection::RIGHT_TO_LEFT,
      outer_size, inner_size);
  EXPECT_EQ(30, offset.left);
  EXPECT_EQ(20, offset.top);
}

}  // namespace

}  // namespace blink
