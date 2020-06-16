// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// NGGridTrackCollectionBase provides an implementation for some shared
// functionality on track range collections, specifically binary search on
// the collection to get a range index given a track number.
class CORE_EXPORT NGGridTrackCollectionBase {
 public:
  static constexpr wtf_size_t kInvalidRangeIndex = kNotFound;
  static constexpr wtf_size_t kMaxRangeIndex = kNotFound - 1;

  class CORE_EXPORT RangeRepeatIterator {
   public:
    RangeRepeatIterator(const NGGridTrackCollectionBase* collection,
                        wtf_size_t range_index);

    // Moves iterator to next range, skipping over repeats in a range. Return
    // true if the move was successful.
    bool MoveToNextRange();
    wtf_size_t RepeatCount() const;
    // Returns the track number for the start of the range.
    wtf_size_t RangeTrackStart() const;
    // Returns the track number at the end of the range.
    wtf_size_t RangeTrackEnd() const;

    bool IsRangeCollapsed() const;

   private:
    bool SetRangeIndex(wtf_size_t range_index);
    const NGGridTrackCollectionBase* collection_;
    wtf_size_t range_index_;
    wtf_size_t range_count_;

    // First track number of a range.
    wtf_size_t range_track_start_;
    // Count of repeated tracks in a range.
    wtf_size_t range_track_count_;
  };

  // Gets the range index for the range that contains the given track number.
  wtf_size_t RangeIndexFromTrackNumber(wtf_size_t track_number) const;

  String ToString() const;

 protected:
  // Returns the first track number of a range.
  virtual wtf_size_t RangeTrackNumber(wtf_size_t range_index) const = 0;

  // Returns the number of tracks in a range.
  virtual wtf_size_t RangeTrackCount(wtf_size_t range_index) const = 0;

  // Returns true if the range at the given index is collapsed.
  virtual bool IsRangeCollapsed(wtf_size_t range_index) const = 0;

  // Returns the number of track ranges in the collection.
  virtual wtf_size_t RangeCount() const = 0;
};

// Stores tracks related data by compressing repeated tracks into a single node.
struct NGGridTrackRepeater {
  enum class RepeatType { kCount, kAutoFill, kAutoFit };
  NGGridTrackRepeater(wtf_size_t track_index,
                      wtf_size_t repeat_size,
                      wtf_size_t repeat_count,
                      RepeatType repeat_type);
  String ToString() const;
  bool operator==(const NGGridTrackRepeater& rhs) const;
  // Index of the first track being repeated.
  wtf_size_t track_index;
  // Amount of tracks to be repeated.
  wtf_size_t repeat_size;
  // Amount of times the group of tracks are repeated.
  wtf_size_t repeat_count;
  // Type of repetition.
  RepeatType repeat_type;
};

class CORE_EXPORT NGGridTrackList {
 public:
  NGGridTrackList();
  // Returns the repeat count of the repeater at |index|, or |auto_value|
  // if the repeater is auto.
  wtf_size_t RepeatCount(wtf_size_t index, wtf_size_t auto_value) const;
  // Returns the number of tracks in the repeater at |index|.
  wtf_size_t RepeatSize(wtf_size_t index) const;
  // Returns the repeat type of the repeater at |index|.
  NGGridTrackRepeater::RepeatType RepeatType(wtf_size_t index) const;
  // Returns the count of repeaters.
  wtf_size_t RepeaterCount() const;
  // Returns the total count of all the tracks in this list.
  wtf_size_t TotalTrackCount() const;

  // Adds a non-auto repeater.
  bool AddRepeater(wtf_size_t track_index,
                   wtf_size_t track_count,
                   wtf_size_t repeat_count);
  // Adds an auto repeater.
  bool AddAutoRepeater(wtf_size_t track_index,
                       wtf_size_t track_count,
                       NGGridTrackRepeater::RepeatType repeat_type);
  // Returns true if this list contains an auto repeater.
  bool HasAutoRepeater();

  // Clears all data.
  void Clear();

  String ToString() const;

 private:
  bool AddRepeater(wtf_size_t track_index,
                   wtf_size_t track_count,
                   wtf_size_t repeat_count,
                   NGGridTrackRepeater::RepeatType repeat_type);
  // Returns the amount of tracks available before overflow.
  wtf_size_t AvailableTrackCount() const;

#if DCHECK_IS_ON()
  // Helper to check if |track_index| does not cause a gap or overlap with the
  // tracks in this list. Ensures |track_index| is equal to 1 + the last track's
  // index.
  bool IsTrackContiguous(wtf_size_t track_index) const;
#endif

  Vector<NGGridTrackRepeater> repeaters_;
  // The index of the automatic repeater, if there is one; |kInvalidRangeIndex|
  // otherwise.
  wtf_size_t auto_repeater_index_;
  // Total count of tracks.
  wtf_size_t total_track_count_;
};

class CORE_EXPORT NGGridBlockTrackCollection
    : public NGGridTrackCollectionBase {
 public:
  struct Range {
    wtf_size_t starting_track_number;
    wtf_size_t track_count;
    wtf_size_t repeater_index;
    wtf_size_t repeater_offset;
    bool is_collapsed;
    bool is_implicit_range;
  };

  // Sets the specified, implicit tracks, along with a given auto repeat value.
  void SetSpecifiedTracks(const NGGridTrackList& specified_tracks,
                          wtf_size_t auto_repeat_count,
                          const NGGridTrackList& implicit_tracks);
  // Ensures that after FinalizeRanges is called, a range will start at the
  // |track_number|, and a range will end at |track_number| + |span_length|
  void EnsureTrackCoverage(wtf_size_t track_number, wtf_size_t span_length);

  // Build the collection of ranges based on information provided by
  // SetSpecifiedTracks and EnsureTrackCoverage.
  void FinalizeRanges();
  // Returns the range at the given range index.
  const Range& RangeAtRangeIndex(wtf_size_t range_index) const;
  // Returns the range at the given track.
  const Range& RangeAtTrackNumber(wtf_size_t track_number) const;

  String ToString() const;

 protected:
  // Returns the first track number of a range.
  wtf_size_t RangeTrackNumber(wtf_size_t range_index) const override;

  // Returns the number of tracks in a range.
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override;

  // Returns true if the range at |range_index| is collapsed.
  bool IsRangeCollapsed(wtf_size_t range_index) const override;

  // Returns the number of track ranges in the collection.
  wtf_size_t RangeCount() const override;

 private:
  // Returns true if this collection had implicit tracks provided.
  bool HasImplicitTracks() const;
  // Returns the repeat size of the implicit tracks.
  wtf_size_t ImplicitRepeatSize() const;

  bool track_indices_need_sort_ = false;
  wtf_size_t auto_repeat_count_ = 0;

  // Stores the specified and implicit tracks specified by SetSpecifiedTracks.
  NGGridTrackList specified_tracks_;
  NGGridTrackList implicit_tracks_;

  // Starting and ending tracks mark where ranges will start and end.
  // Once the ranges have been built in FinalizeRanges, these are cleared.
  Vector<wtf_size_t> starting_tracks_;
  Vector<wtf_size_t> ending_tracks_;
  Vector<Range> ranges_;
};

}  // namespace blink
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGGridTrackRepeater)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
