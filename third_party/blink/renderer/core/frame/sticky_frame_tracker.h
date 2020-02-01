// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_STICKY_FRAME_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_STICKY_FRAME_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// Keeps track of the viewport position of a frame.
//
// This is for catching ads that don't follow the Better Ads Standard --
// https://www.betterads.org/desktop-large-sticky-ad/.
//
// The heuristic for "sticky" is as follows:
//
// Given the latest two pairs of main frame scroll offset and frame's position
// wrt viewport (a.k.a. viewport offset), if the two main frame scroll offsets
// differ while the viewport offsets are the same, the frame is considered
// sticky.
//
// In addition, we store the latest 10 (|kNumScrollEvents|) main frame scroll
// offsets and viewport offsets, and keep comparing their range. For the latest
// 10 recordings, if the frame has a viewport offsets range smaller than 0.5
// (|kStickyFrameOffsetsRangeToScrollOffsetsRangeThreshold|) of the main frame
// scroll offsets range, then it's considered sticky as well.
//
// Right now we only consider offsets on the vertical dimension. The
// implementation can be extended to cover both dimensions if needed.
class CORE_EXPORT StickyFrameTracker {
 public:
  // Returns whether the |frame_size| is considered large compared to
  // |main_frame_viewport_size|.
  static bool IsLarge(const IntSize& main_frame_viewport_size,
                      const IntSize& frame_size);

  StickyFrameTracker() = default;
  StickyFrameTracker(const StickyFrameTracker&) = delete;
  ~StickyFrameTracker() = default;

  // Called when the frame's position with respect to the viewport may have
  // changed. Returns whether the frame is sticky. |main_frame_scroll_offset|
  // is the scroll position in the main page. |viewport_offset| is the position
  // of the top-left corner of this frame (every StickyFrameTracker will only be
  // tracking one frame) relative to the browser viewport.
  bool UpdateStickyStatus(const IntPoint& main_frame_scroll_offset,
                          const IntPoint& viewport_offset);

 private:
  void SetSticky();

  bool is_sticky_ = false;
  Deque<int> viewport_offsets_;
  Deque<int> main_frame_scroll_offsets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_STICKY_FRAME_TRACKER_H_
