// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/sticky_frame_tracker.h"

#include <algorithm>

namespace blink {

namespace {

constexpr static uint32_t kNumScrollEvents = 10;
constexpr static double kLargeFrameSizeToViewportSizeThreshold = 0.3;
constexpr static double kStickyFrameOffsetsRangeToScrollOffsetsRangeThreshold =
    0.5;

}  // namespace

// static
bool StickyFrameTracker::IsLarge(const IntSize& main_frame_viewport_size,
                                 const IntSize& frame_size) {
  DCHECK(!main_frame_viewport_size.IsEmpty());
  uint64_t main_frame_viewport_area = main_frame_viewport_size.Area();
  uint64_t frame_area = frame_size.Area();

  return frame_area >=
         kLargeFrameSizeToViewportSizeThreshold * main_frame_viewport_area;
}

bool StickyFrameTracker::UpdateStickyStatus(
    const IntPoint& main_frame_scroll_offset,
    const IntPoint& viewport_offset) {
  // If the frame has been detected as sticky, it's always considered sticky.
  if (is_sticky_)
    return true;

  if (viewport_offsets_.size() > 0) {
    // If the scroll offset doesn't change, don't add the new data point.
    if (main_frame_scroll_offsets_.back() == main_frame_scroll_offset.Y())
      return false;

    // If the scroll offset has changed while the viewport offset doesn't
    // change, then it's considered sticky.
    if (viewport_offsets_.back() == viewport_offset.Y()) {
      SetSticky();
      return true;
    }
  }

  viewport_offsets_.push_back(viewport_offset.Y());
  main_frame_scroll_offsets_.push_back(main_frame_scroll_offset.Y());

  // We don't have enough data points to determine the stickiness.
  if (viewport_offsets_.size() < kNumScrollEvents)
    return false;

  if (viewport_offsets_.size() > kNumScrollEvents) {
    viewport_offsets_.pop_front();
    main_frame_scroll_offsets_.pop_front();
  }

  auto viewport_offset_min_max =
      std::minmax_element(viewport_offsets_.begin(), viewport_offsets_.end());
  auto main_frame_scroll_offset_min_max = std::minmax_element(
      main_frame_scroll_offsets_.begin(), main_frame_scroll_offsets_.end());
  int viewport_offset_range =
      *(viewport_offset_min_max.second) - *(viewport_offset_min_max.first);
  int main_frame_scroll_offset_range =
      *(main_frame_scroll_offset_min_max.second) -
      *(main_frame_scroll_offset_min_max.first);

  // Both the scroll offset and viewport offset have changed. We check the
  // latest |kNumScrollEvents| values to see whether the movement of frame is
  // too small compared to the movement of scrolling. If so, we consider the
  // frame to be sticky.
  if (viewport_offset_range <
      main_frame_scroll_offset_range *
          kStickyFrameOffsetsRangeToScrollOffsetsRangeThreshold) {
    SetSticky();
    return true;
  }

  return false;
}

void StickyFrameTracker::SetSticky() {
  is_sticky_ = true;
  viewport_offsets_.clear();
  main_frame_scroll_offsets_.clear();
}

}  // namespace blink
