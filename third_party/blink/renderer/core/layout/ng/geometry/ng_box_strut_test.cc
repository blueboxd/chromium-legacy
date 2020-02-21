// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Ideally, this would be tested by NGBoxStrut::ConvertToPhysical, but
// this has not been implemented yet.
TEST(NGGeometryUnitsTest, ConvertPhysicalStrutToLogical) {
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  NGPhysicalBoxStrut physical{top, right, bottom, left};

  NGBoxStrut logical = physical.ConvertToLogical(
      WritingMode::kHorizontalTb, base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(left, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical = physical.ConvertToLogical(WritingMode::kHorizontalTb,
                                      base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(right, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical = physical.ConvertToLogical(WritingMode::kVerticalLr,
                                      base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(WritingMode::kVerticalLr,
                                      base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(WritingMode::kVerticalRl,
                                      base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);

  logical = physical.ConvertToLogical(WritingMode::kVerticalRl,
                                      base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);
}

TEST(NGGeometryUnitsTest, ConvertLogicalStrutToPhysical) {
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  NGBoxStrut logical(left, right, top, bottom);
  NGBoxStrut converted =
      logical
          .ConvertToPhysical(WritingMode::kHorizontalTb,
                             base::i18n::TextDirection::LEFT_TO_RIGHT)
          .ConvertToLogical(WritingMode::kHorizontalTb,
                            base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kHorizontalTb,
                                     base::i18n::TextDirection::RIGHT_TO_LEFT)
                  .ConvertToLogical(WritingMode::kHorizontalTb,
                                    base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kVerticalLr,
                                     base::i18n::TextDirection::LEFT_TO_RIGHT)
                  .ConvertToLogical(WritingMode::kVerticalLr,
                                    base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kVerticalLr,
                                     base::i18n::TextDirection::RIGHT_TO_LEFT)
                  .ConvertToLogical(WritingMode::kVerticalLr,
                                    base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kVerticalRl,
                                     base::i18n::TextDirection::LEFT_TO_RIGHT)
                  .ConvertToLogical(WritingMode::kVerticalRl,
                                    base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kVerticalRl,
                                     base::i18n::TextDirection::RIGHT_TO_LEFT)
                  .ConvertToLogical(WritingMode::kVerticalRl,
                                    base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kSidewaysRl,
                                     base::i18n::TextDirection::LEFT_TO_RIGHT)
                  .ConvertToLogical(WritingMode::kSidewaysRl,
                                    base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kSidewaysRl,
                                     base::i18n::TextDirection::RIGHT_TO_LEFT)
                  .ConvertToLogical(WritingMode::kSidewaysRl,
                                    base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kSidewaysLr,
                                     base::i18n::TextDirection::LEFT_TO_RIGHT)
                  .ConvertToLogical(WritingMode::kSidewaysLr,
                                    base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(logical, converted);
  converted = logical
                  .ConvertToPhysical(WritingMode::kSidewaysLr,
                                     base::i18n::TextDirection::RIGHT_TO_LEFT)
                  .ConvertToLogical(WritingMode::kSidewaysLr,
                                    base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(logical, converted);
}

}  // namespace

}  // namespace blink
