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
    if (IsRangeCollapsed(range_index)) {
      builder.Append(", Collapsed ");
    }
    builder.Append("]");
    if (range_index + 1 < RangeCount())
      builder.Append(", ");
  }
  return builder.ToString();
}

NGGridTrackCollectionBase::RangeRepeatIterator::RangeRepeatIterator(
    const NGGridTrackCollectionBase* collection,
    wtf_size_t range_index)
    : collection_(collection) {
  DCHECK(collection_);
  range_count_ = collection_->RangeCount();
  SetRangeIndex(range_index);
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::MoveToNextRange() {
  return SetRangeIndex(range_index_ + 1);
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RepeatCount() const {
  return range_track_count_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackStart()
    const {
  return range_track_start_;
}

wtf_size_t NGGridTrackCollectionBase::RangeRepeatIterator::RangeTrackEnd()
    const {
  if (range_index_ == kInvalidRangeIndex)
    return kInvalidRangeIndex;
  return range_track_start_ + range_track_count_ - 1;
}

bool NGGridTrackCollectionBase::RangeRepeatIterator::IsRangeCollapsed() const {
  DCHECK(range_index_ != kInvalidRangeIndex);
  DCHECK(collection_);
  return collection_->IsRangeCollapsed(range_index_);
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

NGGridTrackRepeater::NGGridTrackRepeater(wtf_size_t track_index,
                                         wtf_size_t repeat_size,
                                         wtf_size_t repeat_count,
                                         RepeatType repeat_type)
    : track_index(track_index),
      repeat_size(repeat_size),
      repeat_count(repeat_count),
      repeat_type(repeat_type) {}

String NGGridTrackRepeater::ToString() const {
  StringBuilder builder;
  builder.Append("Repeater: [Index: ");
  builder.AppendNumber<wtf_size_t>(track_index);
  builder.Append("], [RepeatSize: ");
  builder.AppendNumber<wtf_size_t>(repeat_size);
  builder.Append("], [RepeatCount: ");
  switch (repeat_type) {
    case RepeatType::kCount:
      builder.AppendNumber<wtf_size_t>(repeat_count);
      builder.Append("]");
      break;
    case RepeatType::kAutoFill:
      builder.Append("auto-fill]");
      break;
    case RepeatType::kAutoFit:
      builder.Append("auto-fit]");
      break;
  }
  return builder.ToString();
}

NGGridTrackList::NGGridTrackList() {
  auto_repeater_index_ = NGGridTrackCollectionBase::kInvalidRangeIndex;
  total_track_count_ = 0;
}

wtf_size_t NGGridTrackList::RepeatCount(wtf_size_t index,
                                        wtf_size_t auto_value) const {
  DCHECK_LT(index, RepeaterCount());
  if (index == auto_repeater_index_)
    return auto_value;
  return repeaters_[index].repeat_count;
}

wtf_size_t NGGridTrackList::RepeatSize(wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_size;
}

NGGridTrackRepeater::RepeatType NGGridTrackList::RepeatType(
    wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_type;
}

wtf_size_t NGGridTrackList::RepeaterCount() const {
  return repeaters_.size();
}

wtf_size_t NGGridTrackList::TotalTrackCount() const {
  return total_track_count_;
}

bool NGGridTrackList::AddRepeater(wtf_size_t track_index,
                                  wtf_size_t track_count,
                                  wtf_size_t repeat_count) {
  return AddRepeater(track_index, track_count, repeat_count,
                     NGGridTrackRepeater::RepeatType::kCount);
}

bool NGGridTrackList::AddAutoRepeater(
    wtf_size_t track_index,
    wtf_size_t track_count,
    NGGridTrackRepeater::RepeatType repeat_type) {
  return AddRepeater(track_index, track_count, 1u, repeat_type);
}

bool NGGridTrackList::AddRepeater(wtf_size_t track_index,
                                  wtf_size_t track_count,
                                  wtf_size_t repeat_count,
                                  NGGridTrackRepeater::RepeatType repeat_type) {
  // Ensure valid track index.
  DCHECK_NE(NGGridTrackCollectionBase::kInvalidRangeIndex, track_index);

#if DCHECK_IS_ON()
  // Ensure we do not skip or overlap tracks.
  DCHECK(IsTrackContiguous(track_index));
#endif

  // If the repeater is auto, the repeat_count should be 1.
  DCHECK(repeat_type == NGGridTrackRepeater::RepeatType::kCount ||
         repeat_count == 1u);

  // Ensure adding tracks will not overflow the total in this track list and
  // that there is only one auto repeater per track list.
  switch (repeat_type) {
    case NGGridTrackRepeater::RepeatType::kCount:
      if (track_count > AvailableTrackCount() / repeat_count)
        return false;
      total_track_count_ += track_count * repeat_count;
      break;
    case NGGridTrackRepeater::RepeatType::kAutoFill:
    case NGGridTrackRepeater::RepeatType::kAutoFit:  // Intentional Fallthrough.
      if (HasAutoRepeater() || track_count > AvailableTrackCount())
        return false;
      total_track_count_ += track_count;
      // Update auto repeater index and append repeater.
      auto_repeater_index_ = repeaters_.size();
      break;
  }

  repeaters_.emplace_back(track_index, track_count, repeat_count, repeat_type);

  return true;
}

String NGGridTrackList::ToString() const {
  StringBuilder builder;
  builder.Append("TrackList: {");
  for (wtf_size_t i = 0; i < repeaters_.size(); ++i) {
    builder.Append(" ");
    builder.Append(repeaters_[i].ToString());
    if (i + 1 != repeaters_.size())
      builder.Append(", ");
  }
  builder.Append(" } ");
  return builder.ToString();
}

bool NGGridTrackList::HasAutoRepeater() {
  return auto_repeater_index_ != NGGridTrackCollectionBase::kInvalidRangeIndex;
}

wtf_size_t NGGridTrackList::AvailableTrackCount() const {
  return NGGridTrackCollectionBase::kMaxRangeIndex - total_track_count_;
}

#if DCHECK_IS_ON()
bool NGGridTrackList::IsTrackContiguous(wtf_size_t track_index) const {
  return repeaters_.IsEmpty() ||
         (repeaters_.back().track_index + repeaters_.back().repeat_size ==
          track_index);
}
#endif

void NGGridBlockTrackCollection::SetSpecifiedTracks(
    const NGGridTrackList& specified_tracks,
    wtf_size_t auto_repeat_count,
    const NGGridTrackList& implicit_tracks) {
  // The implicit track list should have only one repeater, if any.
  DCHECK_LE(implicit_tracks.RepeaterCount(), 1u);
  specified_tracks_ = specified_tracks;
  implicit_tracks_ = implicit_tracks;
  auto_repeat_count_ = auto_repeat_count;

  wtf_size_t repeater_count = specified_tracks_.RepeaterCount();
  wtf_size_t total_track_count = 0;

  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    wtf_size_t repeater_track_start = total_track_count + 1;
    wtf_size_t repeater_track_count =
        specified_tracks_.RepeatCount(i, auto_repeat_count_) *
        specified_tracks_.RepeatSize(i);
    if (repeater_track_count != 0) {
      starting_tracks_.push_back(repeater_track_start);
      ending_tracks_.push_back(repeater_track_start + repeater_track_count - 1);
    }
    total_track_count += repeater_track_count;
  }
}

void NGGridBlockTrackCollection::EnsureTrackCoverage(wtf_size_t track_number,
                                                     wtf_size_t span_length) {
  DCHECK_NE(kInvalidRangeIndex, track_number);
  DCHECK_NE(kInvalidRangeIndex, span_length);
  track_indices_need_sort_ = true;
  starting_tracks_.push_back(track_number);
  ending_tracks_.push_back(track_number + span_length - 1);
}

void NGGridBlockTrackCollection::FinalizeRanges() {
  ranges_.clear();

  // Sort start and ending tracks from low to high.
  if (track_indices_need_sort_) {
    std::stable_sort(starting_tracks_.begin(), starting_tracks_.end());
    std::stable_sort(ending_tracks_.begin(), ending_tracks_.end());
  }

  wtf_size_t current_range_track_start = 1u;
  if (starting_tracks_.size() > 0 && starting_tracks_[0] == 0)
    current_range_track_start = 0;

  // Indices into the starting and ending track vectors.
  wtf_size_t starting_tracks_index = 0;
  wtf_size_t ending_tracks_index = 0;

  wtf_size_t repeater_index = kInvalidRangeIndex;
  wtf_size_t repeater_track_start = kInvalidRangeIndex;
  wtf_size_t next_repeater_track_start = 1u;
  wtf_size_t current_repeater_track_count = 0;

  wtf_size_t total_repeater_count = specified_tracks_.RepeaterCount();
  wtf_size_t open_items_or_repeaters = 0;
  bool is_in_auto_fit_range = false;

  while (true) {
    // Identify starting tracks index.
    while (starting_tracks_index < starting_tracks_.size() &&
           current_range_track_start >=
               starting_tracks_[starting_tracks_index]) {
      ++starting_tracks_index;
      ++open_items_or_repeaters;
    }

    // Identify ending tracks index.
    while (ending_tracks_index < ending_tracks_.size() &&
           current_range_track_start > ending_tracks_[ending_tracks_index]) {
      ++ending_tracks_index;
      --open_items_or_repeaters;
      DCHECK_GE(open_items_or_repeaters, 0u);
    }

    // Identify ending tracks index.
    if (ending_tracks_index >= ending_tracks_.size()) {
      DCHECK_EQ(open_items_or_repeaters, 0u);
      break;
    }

    // Determine the next starting and ending track index.
    wtf_size_t next_starting_track = kInvalidRangeIndex;
    if (starting_tracks_index < starting_tracks_.size())
      next_starting_track = starting_tracks_[starting_tracks_index];
    wtf_size_t next_ending_track = ending_tracks_[ending_tracks_index];

    // Move |next_repeater_track_start| to the start of the next repeater, if
    // needed.
    while (current_range_track_start == next_repeater_track_start) {
      if (++repeater_index == total_repeater_count) {
        repeater_index = kInvalidRangeIndex;
        repeater_track_start = next_repeater_track_start;
        is_in_auto_fit_range = false;
        break;
      }

      is_in_auto_fit_range = specified_tracks_.RepeatType(repeater_index) ==
                             NGGridTrackRepeater::RepeatType::kAutoFit;
      current_repeater_track_count =
          specified_tracks_.RepeatCount(repeater_index, auto_repeat_count_) *
          specified_tracks_.RepeatSize(repeater_index);
      repeater_track_start = next_repeater_track_start;
      next_repeater_track_start += current_repeater_track_count;
    }

    // Determine track number and count of the range.
    Range range;
    range.starting_track_number = current_range_track_start;
    if (next_starting_track != kInvalidRangeIndex) {
      range.track_count =
          std::min(next_ending_track + 1u, next_starting_track) -
          current_range_track_start;
    } else {
      range.track_count = next_ending_track + 1u - current_range_track_start;
    }

    // Compute repeater index and offset.
    if (repeater_index == kInvalidRangeIndex) {
      range.is_implicit_range = true;
      if (implicit_tracks_.RepeaterCount() == 0) {
        // No specified implicit tracks, use auto tracks.
        range.repeater_index = kInvalidRangeIndex;
        range.repeater_offset = 0;
      } else {
        // Use implicit tracks.
        wtf_size_t implicit_repeat_size = ImplicitRepeatSize();
        range.repeater_index = 0;
        if (range.starting_track_number == 0) {
          wtf_size_t offset_from_end =
              (1 - range.starting_track_number) % implicit_repeat_size;
          if (offset_from_end == implicit_repeat_size) {
            range.repeater_offset = 0;
          } else {
            range.repeater_offset =
                current_range_track_start - repeater_track_start;
          }
        }
      }
    } else {
      range.is_implicit_range = false;
      range.repeater_index = repeater_index;
      range.repeater_offset = current_range_track_start - repeater_track_start;
    }
    range.is_collapsed = is_in_auto_fit_range && open_items_or_repeaters == 1u;

    current_range_track_start += range.track_count;
    ranges_.push_back(range);
  }

#if DCHECK_IS_ON()
  while (repeater_index != kInvalidRangeIndex &&
         repeater_index < total_repeater_count - 1u) {
    ++repeater_index;
    DCHECK_EQ(0u, specified_tracks_.RepeatSize(repeater_index));
  }
#endif
  DCHECK_EQ(starting_tracks_index, starting_tracks_.size());
  DCHECK_EQ(ending_tracks_index, starting_tracks_.size());
  DCHECK(repeater_index == total_repeater_count - 1u ||
         repeater_index == kInvalidRangeIndex);
  starting_tracks_.clear();
  ending_tracks_.clear();
}

const NGGridBlockTrackCollection::Range&
NGGridBlockTrackCollection::RangeAtRangeIndex(wtf_size_t range_index) const {
  DCHECK_NE(range_index, kInvalidRangeIndex);
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index];
}
const NGGridBlockTrackCollection::Range&
NGGridBlockTrackCollection::RangeAtTrackNumber(wtf_size_t track_number) const {
  wtf_size_t range_index = RangeIndexFromTrackNumber(track_number);
  DCHECK_NE(range_index, kInvalidRangeIndex);
  DCHECK_LT(range_index, ranges_.size());
  return ranges_[range_index];
}

String NGGridBlockTrackCollection::ToString() const {
  if (ranges_.IsEmpty()) {
    StringBuilder builder;
    builder.Append("NGGridTrackCollection: [SpecifiedTracks: ");
    builder.Append(specified_tracks_.ToString());
    if (HasImplicitTracks()) {
      builder.Append("], [ImplicitTracks: ");
      builder.Append(implicit_tracks_.ToString());
    }

    builder.Append("], [Starting: {");
    for (wtf_size_t i = 0; i < starting_tracks_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(starting_tracks_[i]);
      if (i + 1 != starting_tracks_.size())
        builder.Append(", ");
    }
    builder.Append("} ], [Ending: {");
    for (wtf_size_t i = 0; i < ending_tracks_.size(); ++i) {
      builder.AppendNumber<wtf_size_t>(ending_tracks_[i]);
      if (i + 1 != ending_tracks_.size())
        builder.Append(", ");
    }
    builder.Append("} ] ");
    return builder.ToString();
  } else {
    return NGGridTrackCollectionBase::ToString();
  }
}
bool NGGridBlockTrackCollection::HasImplicitTracks() const {
  return implicit_tracks_.RepeaterCount() != 0;
}
wtf_size_t NGGridBlockTrackCollection::ImplicitRepeatSize() const {
  DCHECK(HasImplicitTracks());
  return implicit_tracks_.RepeatSize(0);
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackNumber(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].starting_track_number;
}

wtf_size_t NGGridBlockTrackCollection::RangeTrackCount(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].track_count;
}

bool NGGridBlockTrackCollection::IsRangeCollapsed(
    wtf_size_t range_index) const {
  DCHECK_LT(range_index, RangeCount());
  return ranges_[range_index].is_collapsed;
}

wtf_size_t NGGridBlockTrackCollection::RangeCount() const {
  return ranges_.size();
}

}  // namespace blink
