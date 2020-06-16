// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {
namespace {
#define EXPECT_RANGE(expected_start, expected_count, iterator)              \
  EXPECT_EQ(expected_count, iterator.RepeatCount());                        \
  EXPECT_EQ(expected_start, iterator.RangeTrackStart());                    \
  EXPECT_EQ(expected_start + expected_count - 1, iterator.RangeTrackEnd()); \
  EXPECT_FALSE(iterator.IsRangeCollapsed());
#define EXPECT_COLLAPSED_RANGE(expected_start, expected_count, iterator)    \
  EXPECT_EQ(expected_start, iterator.RangeTrackStart());                    \
  EXPECT_EQ(expected_count, iterator.RepeatCount());                        \
  EXPECT_EQ(expected_start + expected_count - 1, iterator.RangeTrackEnd()); \
  EXPECT_TRUE(iterator.IsRangeCollapsed());
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
  bool IsRangeCollapsed(wtf_size_t range_index) const override { return false; }

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
  EXPECT_RANGE(0u, 3u, iterator);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(3u, 10u, iterator);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(13u, 5u, iterator);

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

TEST_F(NGGridTrackCollectionTest, TestNGGridTrackList) {
  NGGridTrackList track_list;
  ASSERT_EQ(0u, track_list.RepeaterCount());
  EXPECT_FALSE(track_list.HasAutoRepeater());

  EXPECT_TRUE(track_list.AddRepeater(0, 2, 4));
  ASSERT_EQ(1u, track_list.RepeaterCount());
  EXPECT_EQ(8u, track_list.TotalTrackCount());
  EXPECT_EQ(4u, track_list.RepeatCount(0, 77));
  EXPECT_EQ(2u, track_list.RepeatSize(0));
  EXPECT_FALSE(track_list.HasAutoRepeater());

  EXPECT_TRUE(track_list.AddAutoRepeater(
      2, 3, NGGridTrackRepeater::RepeatType::kAutoFill));
  ASSERT_EQ(2u, track_list.RepeaterCount());
  EXPECT_EQ(11u, track_list.TotalTrackCount());
  EXPECT_EQ(77u, track_list.RepeatCount(1, 77));
  EXPECT_EQ(3u, track_list.RepeatSize(1));
  EXPECT_TRUE(track_list.HasAutoRepeater());

  // Can't add more than one auto repeater to a list.
  EXPECT_FALSE(track_list.AddAutoRepeater(
      5, 3, NGGridTrackRepeater::RepeatType::kAutoFill));

  EXPECT_TRUE(track_list.AddRepeater(
      5, NGGridTrackCollectionBase::kMaxRangeIndex - 20, 1));
  ASSERT_EQ(3u, track_list.RepeaterCount());
  EXPECT_EQ(NGGridTrackCollectionBase::kMaxRangeIndex - 9,
            track_list.TotalTrackCount());
  EXPECT_EQ(1u, track_list.RepeatCount(2, 77));
  EXPECT_EQ(NGGridTrackCollectionBase::kMaxRangeIndex - 20,
            track_list.RepeatSize(2));

  // Try to add a repeater that would overflow the total track count.
  EXPECT_FALSE(track_list.AddRepeater(
      NGGridTrackCollectionBase::kMaxRangeIndex - 15u, 3, 10));
  ASSERT_EQ(3u, track_list.RepeaterCount());

  // Try to add a repeater that would overflow the track size in a repeater.
  EXPECT_FALSE(
      track_list.AddRepeater(NGGridTrackCollectionBase::kMaxRangeIndex - 15u,
                             NGGridTrackCollectionBase::kMaxRangeIndex, 10));
  ASSERT_EQ(3u, track_list.RepeaterCount());
}

TEST_F(NGGridTrackCollectionTest, TestNGGridBlockTrackCollection) {
  NGGridTrackList specified_tracks;
  ASSERT_TRUE(specified_tracks.AddRepeater(1, 2, 4));
  ASSERT_TRUE(specified_tracks.AddAutoRepeater(
      3, 3, NGGridTrackRepeater::RepeatType::kAutoFill));
  ASSERT_EQ(2u, specified_tracks.RepeaterCount());
  NGGridBlockTrackCollection block_collection;
  block_collection.SetSpecifiedTracks(specified_tracks, 3, NGGridTrackList());
  block_collection.FinalizeRanges();

  NGGridTrackCollectionBase::RangeRepeatIterator iterator(&block_collection,
                                                          0u);
  EXPECT_RANGE(1u, 8u, iterator);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(9u, 9u, iterator);

  EXPECT_FALSE(iterator.MoveToNextRange());
}

TEST_F(NGGridTrackCollectionTest, TestNGGridBlockTrackCollectionCollapsed) {
  NGGridTrackList specified_tracks;
  ASSERT_TRUE(specified_tracks.AddRepeater(1, 2, 4));
  ASSERT_TRUE(specified_tracks.AddAutoRepeater(
      3, 3, NGGridTrackRepeater::RepeatType::kAutoFit));
  ASSERT_TRUE(specified_tracks.AddRepeater(6, 3, 7));
  ASSERT_EQ(3u, specified_tracks.RepeaterCount());
  NGGridBlockTrackCollection block_collection;
  block_collection.SetSpecifiedTracks(specified_tracks, 3, NGGridTrackList());
  block_collection.FinalizeRanges();

  NGGridTrackCollectionBase::RangeRepeatIterator iterator(&block_collection,
                                                          0u);
  EXPECT_RANGE(1u, 8u, iterator);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_COLLAPSED_RANGE(9u, 9u, iterator);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(18u, 21u, iterator);

  EXPECT_FALSE(iterator.MoveToNextRange());
}

TEST_F(NGGridTrackCollectionTest, TestNGGridBlockTrackCollectionImplicit) {
  NGGridTrackList specified_tracks;
  ASSERT_TRUE(specified_tracks.AddRepeater(1, 2, 4));
  ASSERT_TRUE(specified_tracks.AddRepeater(3, 3, 3));
  ASSERT_TRUE(specified_tracks.AddRepeater(6, 3, 7));
  ASSERT_EQ(3u, specified_tracks.RepeaterCount());

  NGGridTrackList implicit_tracks;
  ASSERT_TRUE(implicit_tracks.AddRepeater(1, 8, 2));

  NGGridBlockTrackCollection block_collection;
  block_collection.SetSpecifiedTracks(specified_tracks, 3, implicit_tracks);
  block_collection.EnsureTrackCoverage(3, 40);
  block_collection.EnsureTrackCoverage(3, 40);
  block_collection.FinalizeRanges();
  NGGridTrackCollectionBase::RangeRepeatIterator iterator(&block_collection,
                                                          0u);
  EXPECT_RANGE(1u, 2u, iterator);
  EXPECT_FALSE(block_collection.RangeAtTrackNumber(1u).is_implicit_range);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(3u, 6u, iterator);
  EXPECT_FALSE(block_collection.RangeAtTrackNumber(4).is_implicit_range);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(9u, 9u, iterator);
  EXPECT_FALSE(block_collection.RangeAtTrackNumber(7).is_implicit_range);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_RANGE(18u, 21u, iterator);
  EXPECT_FALSE(block_collection.RangeAtTrackNumber(20).is_implicit_range);

  EXPECT_TRUE(iterator.MoveToNextRange());
  EXPECT_TRUE(block_collection.RangeAtTrackNumber(40).is_implicit_range);
  EXPECT_RANGE(39u, 4u, iterator);

  EXPECT_FALSE(iterator.MoveToNextRange());
}

}  // namespace

}  // namespace blink
