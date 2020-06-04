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

  class CORE_EXPORT RangeRepeatIterator {
   public:
    RangeRepeatIterator(NGGridTrackCollectionBase* collection,
                        wtf_size_t range_index);

    // Moves iterator to next range, skipping over repeats in a range. Return
    // true if the move was successful.
    bool MoveToNextRange();
    wtf_size_t RepeatCount();
    // Returns the track number for the start of the range.
    wtf_size_t RangeTrackStart();
    // Returns the track number at the end of the range.
    wtf_size_t RangeTrackEnd();

   private:
    bool SetRangeIndex(wtf_size_t range_index);
    NGGridTrackCollectionBase* collection_;
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

  // Returns the number of track ranges in the collection.
  virtual wtf_size_t RangeCount() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
