// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {
namespace {

class NGGridTrackCollectionBaseTest : public NGGridTrackCollectionBase {
 public:
  struct TestTrackRange {
    wtf_size_t track_number;
    wtf_size_t track_count;
  };
  explicit NGGridTrackCollectionBaseTest(
      const std::vector<wtf_size_t>& range_sizes) {
    wtf_size_t track_number = 0;
    for (wtf_size_t size : range_sizes) {
      TestTrackRange range;
      range.track_number = track_number;
      range.track_count = size;
      ranges_.push_back(range);
      track_number += size;
    }
  }

 protected:
  wtf_size_t RangeTrackNumber(wtf_size_t range_index) const override {
    return ranges_[range_index].track_number;
  }
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override {
    return ranges_[range_index].track_count;
  }
  wtf_size_t RangeCount() const override { return ranges_.size(); }

 private:
  Vector<TestTrackRange> ranges_;
};

using NGGridTrackCollectionTest = NGLayoutTest;

TEST_F(NGGridTrackCollectionTest, TestRangeIndexFromTrackNumber) {
  // Small case.
  NGGridTrackCollectionBaseTest track_collection({3, 10u, 5u});
  EXPECT_EQ(0u, track_collection.RangeIndexFromTrackNumber(0u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromTrackNumber(4u));
  EXPECT_EQ(2u, track_collection.RangeIndexFromTrackNumber(15u));

  // Small case with large repeat count.
  track_collection = NGGridTrackCollectionBaseTest({3000000u, 7u, 10u});
  EXPECT_EQ(0u, track_collection.RangeIndexFromTrackNumber(600u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromTrackNumber(3000000u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromTrackNumber(3000004u));

  // Larger case.
  track_collection = NGGridTrackCollectionBaseTest({
      10u,   // 0 - 9
      10u,   // 10 - 19
      10u,   // 20 - 29
      10u,   // 30 - 39
      20u,   // 40 - 59
      20u,   // 60 - 79
      20u,   // 80 - 99
      100u,  // 100 - 199
  });
  EXPECT_EQ(0u, track_collection.RangeIndexFromTrackNumber(0u));
  EXPECT_EQ(3u, track_collection.RangeIndexFromTrackNumber(35u));
  EXPECT_EQ(4u, track_collection.RangeIndexFromTrackNumber(40u));
  EXPECT_EQ(5u, track_collection.RangeIndexFromTrackNumber(79));
  EXPECT_EQ(7u, track_collection.RangeIndexFromTrackNumber(105u));
}

TEST_F(NGGridTrackCollectionTest, TestRangeRepeatIteratorMoveNext) {
  // [1-3] [4-13] [14 -18]
  NGGridTrackCollectionBaseTest track_collection({3u, 10u, 5u});
  EXPECT_EQ(0u, track_collection.RangeIndexFromTrackNumber(0u));

  NGGridTrackCollectionBaseTest::RangeRepeatIterator iterator(&track_collection,
                                                              0u);
  EXPECT_EQ(3u, iterator.RepeatCount());
  EXPECT_EQ(0u, iterator.RangeTrackStart());
  EXPECT_EQ(2u, iterator.RangeTrackEnd());

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_EQ(10u, iterator.RepeatCount());
  EXPECT_EQ(3u, iterator.RangeTrackStart());
  EXPECT_EQ(12u, iterator.RangeTrackEnd());

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_EQ(5u, iterator.RepeatCount());
  EXPECT_EQ(13u, iterator.RangeTrackStart());
  EXPECT_EQ(17u, iterator.RangeTrackEnd());

  EXPECT_FALSE(iterator.MoveToNextRange());

  NGGridTrackCollectionBaseTest empty_collection({});

  NGGridTrackCollectionBaseTest::RangeRepeatIterator empty_iterator(
      &empty_collection, 0u);
  EXPECT_EQ(NGGridTrackCollectionBase::kInvalidRangeIndex,
            empty_iterator.RangeTrackStart());
  EXPECT_EQ(NGGridTrackCollectionBase::kInvalidRangeIndex,
            empty_iterator.RangeTrackEnd());
  EXPECT_EQ(0u, empty_iterator.RepeatCount());
  EXPECT_FALSE(empty_iterator.MoveToNextRange());
}

}  // namespace

}  // namespace blink
