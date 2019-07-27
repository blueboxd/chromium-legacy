// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

class DocumentLayoutTest : public testing::Test {
 protected:
  DocumentLayout layout_;
};

using DocumentLayoutDeathTest = DocumentLayoutTest;

// TODO(kmoon): Need to use this with EXPECT_PRED2 instead of just using
// EXPECT_EQ, due to ADL issues with pp::Size's operator== (defined in global
// namespace, instead of in "pp").
inline bool PpSizeEq(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs == rhs;
}

TEST_F(DocumentLayoutTest, DefaultConstructor) {
  EXPECT_EQ(layout_.default_page_orientation(), 0);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 0));
}

TEST_F(DocumentLayoutTest, CopyConstructor) {
  layout_.RotatePagesClockwise();
  layout_.EnlargeHeight(2);

  DocumentLayout copy(layout_);
  EXPECT_EQ(copy.default_page_orientation(), 1);
  EXPECT_PRED2(PpSizeEq, copy.size(), pp::Size(0, 2));

  layout_.RotatePagesClockwise();
  layout_.EnlargeHeight(5);
  EXPECT_EQ(copy.default_page_orientation(), 1);
  EXPECT_PRED2(PpSizeEq, copy.size(), pp::Size(0, 2));
}

TEST_F(DocumentLayoutTest, CopyAssignment) {
  layout_.RotatePagesClockwise();
  layout_.EnlargeHeight(2);

  DocumentLayout copy;
  EXPECT_EQ(copy.default_page_orientation(), 0);
  EXPECT_PRED2(PpSizeEq, copy.size(), pp::Size(0, 0));

  copy = layout_;
  EXPECT_EQ(copy.default_page_orientation(), 1);
  EXPECT_PRED2(PpSizeEq, copy.size(), pp::Size(0, 2));

  layout_.RotatePagesClockwise();
  layout_.EnlargeHeight(5);
  EXPECT_EQ(copy.default_page_orientation(), 1);
  EXPECT_PRED2(PpSizeEq, copy.size(), pp::Size(0, 2));
}

TEST_F(DocumentLayoutTest, RotatePagesClockwise) {
  layout_.RotatePagesClockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 1);

  layout_.RotatePagesClockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 2);

  layout_.RotatePagesClockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 3);

  layout_.RotatePagesClockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 0);
}

TEST_F(DocumentLayoutTest, RotatePagesCounterclockwise) {
  layout_.RotatePagesCounterclockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 3);

  layout_.RotatePagesCounterclockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 2);

  layout_.RotatePagesCounterclockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 1);

  layout_.RotatePagesCounterclockwise();
  EXPECT_EQ(layout_.default_page_orientation(), 0);
}

TEST_F(DocumentLayoutTest, RotatePagesDoesNotRecomputeLayout) {
  layout_.EnlargeHeight(2);

  layout_.RotatePagesClockwise();
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 2));

  layout_.RotatePagesCounterclockwise();
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 2));
}

TEST_F(DocumentLayoutTest, EnlargeHeight) {
  layout_.EnlargeHeight(5);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 5));

  layout_.EnlargeHeight(11);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 16));
}

TEST_F(DocumentLayoutDeathTest, EnlargeHeightNegativeIncrement) {
  EXPECT_DCHECK_DEATH(layout_.EnlargeHeight(-5));
}

TEST_F(DocumentLayoutTest, AppendPageRect) {
  layout_.AppendPageRect(pp::Size(3, 5));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(3, 5));

  layout_.AppendPageRect(pp::Size(7, 11));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(7, 16));

  layout_.AppendPageRect(pp::Size(5, 11));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(7, 27));
}

}  // namespace

}  // namespace chrome_pdf
