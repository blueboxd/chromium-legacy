// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_

namespace upboarding {
namespace stats {

// Event to track image loading metrics.
enum class ImageLoadingEvent {
  // Starts to fetch image in reduced mode background task.
  kStartPrefetch = 0,
  kPrefetchSuccess = 1,
  kPrefetchFailure = 2,
  kMaxValue = kPrefetchFailure,
};

// Records an image loading event.
void RecordImageLoading(ImageLoadingEvent event);

}  // namespace stats
}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
