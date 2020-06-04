// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

constexpr wtf_size_t NGGridTrackCollectionBase::kInvalidRangeIndex;

wtf_size_t NGGridTrackCollectionBase::RangeIndexFromTrackNumber(
    wtf_size_t track_number) const {
  wtf_size_t upper = RangeCount();
  wtf_size_t lower = 0u;

  // We can't look for a range in a collection with no ranges.
  DCHECK_NE(upper, 0u);
  // We don't expect a |track_number| outside of the bounds of the collection.
  DCHECK_NE(track_number, kInvalidRangeIndex);
  DCHECK_LT(track_number,
            RangeTrackNumber(upper - 1u) + RangeTrackCount(upper - 1u));

  // Do a binary search on the tracks.
  wtf_size_t range = upper - lower;
  while (range > 1) {
    wtf_size_t center = lower + (range / 2u);

    wtf_size_t center_track_number = RangeTrackNumber(center);
    wtf_size_t center_track_count = RangeTrackCount(center);

    if (center_track_number <= track_number &&
        (track_number - center_track_number) < center_track_count) {
      // We found the track.
      return center;
    } else if (center_track_number > track_number) {
      // This track is too high.
      upper = center;
      range = upper - lower;
    } else {
      // This track is too low.
      lower = center + 1u;
      range = upper - lower;
    }
  }

  return lower;
}

String NGGridTrackCollectionBase::ToString() const {
  if (RangeCount() == kInvalidRangeIndex)
    return "NGGridTrackCollection: Empty";

  StringBuilder builder;
  builder.Append("NGGridTrackCollection: [RangeCount: ");
  builder.AppendNumber<wtf_size_t>(RangeCount());
  builder.Append("], Ranges: ");
  for (wtf_size_t range_index = 0; range_index < RangeCount(); ++range_index) {
    builder.Append("[Start: ");
    builder.AppendNumber<wtf_size_t>(RangeTrackNumber(range_index));
    builder.Append(", Count: ");
    builder.AppendNumber<wtf_size_t>(RangeTrackCount(range_index));
    builder.Append("]");
    if (range_index + 1 < RangeCount())
      builder.Append(", ");
  }
  return builder.ToString();
}

NGGridTrackCollectionBase::RangeRepeatIterator::RangeRepeatIterator(
    NGGridTrackCollectionBase* collection,
    wtf_size_t range_index)
    : collection_(collection) {
  DCHECK(collection_);
  range_count_ = collection_->RangeCount();
  SetRangeIndex(range_index);
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::MoveToNextRange() {
  return SetRangeIndex(range_index_ + 1);
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RepeatCount() {
  return range_track_count_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackStart() {
  return range_track_start_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackEnd() {
  if (range_index_ == kInvalidRangeIndex)
    return kInvalidRangeIndex;
  return range_track_start_ + range_track_count_ - 1;
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::SetRangeIndex(
    wtf_size_t range_index) {
  if (range_index < 0 || range_index >= range_count_) {
    // Invalid index.
    range_index_ = kInvalidRangeIndex;
    range_track_start_ = kInvalidRangeIndex;
    range_track_count_ = 0;
    return false;
  }

  range_index_ = range_index;
  range_track_start_ = collection_->RangeTrackNumber(range_index_);
  range_track_count_ = collection_->RangeTrackCount(range_index_);
  return true;
}

}  // namespace blink
