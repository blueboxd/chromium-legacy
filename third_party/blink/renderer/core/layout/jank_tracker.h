// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_JANK_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_JANK_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/jank_region.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IntRect;
class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class TracedValue;
class WebInputEvent;

// Tracks "jank" from layout objects changing their visual location between
// animation frames.
class CORE_EXPORT JankTracker {
  USING_FAST_MALLOC(JankTracker);

 public:
  JankTracker(LocalFrameView*);
  ~JankTracker() {}
  void NotifyObjectPrePaint(const LayoutObject& object,
                            const PropertyTreeState& property_tree_state,
                            const IntRect& old_visual_rect,
                            const IntRect& new_visual_rect);
  void NotifyCompositedLayerMoved(const LayoutObject& object,
                                  FloatRect old_layer_rect,
                                  FloatRect new_layer_rect);
  void NotifyPrePaintFinished();
  void NotifyInput(const WebInputEvent&);
  void NotifyScroll(ScrollType);
  bool IsActive();
  double Score() const { return score_; }
  double ScoreWithMoveDistance() const { return score_with_move_distance_; }
  double WeightedScore() const { return weighted_score_; }
  float OverallMaxDistance() const { return overall_max_distance_; }
  bool ObservedInputOrScroll() const { return observed_input_or_scroll_; }
  void Dispose() { timer_.Stop(); }

 private:
  void AccumulateJank(const LayoutObject&,
                      const PropertyTreeState&,
                      FloatRect old_rect,
                      FloatRect new_rect);
  void TimerFired(TimerBase*) {}
  std::unique_ptr<TracedValue> PerFrameTraceData(
      double jank_fraction,
      double jank_fraction_with_move_distance,
      double granularity_scale) const;
  double SubframeWeightingFactor() const;
  std::vector<gfx::Rect> ConvertIntRectsToGfxRects(
      const Vector<IntRect>& int_rects,
      double granularity_scale);
  void SetLayoutShiftRects(const Vector<IntRect>& int_rects,
                           double granularity_scale,
                           bool using_sweep_line);

  // This owns us.
  UntracedMember<LocalFrameView> frame_view_;

  // The cumulative jank score for this LocalFrame, unweighted.
  double score_;

  // The cumulative jank score for this LocalFrame, unweighted, with move
  // distance applied. This is a temporary member needed to understand the
  // impact of move distance on scores, and will be removed once analysis is
  // complete.
  double score_with_move_distance_;

  // The cumulative jank score for this LocalFrame, with each increase weighted
  // by the extent to which the LocalFrame visibly occupied the main frame at
  // the time the jank occurred (e.g. x0.5 if the subframe occupied half of the
  // main frame's reported size (see JankTracker::SubframeWeightingFactor).
  double weighted_score_;

  // The per-animation-frame jank region.
  Region region_;

  // Experimental jank region implementation using sweep-line algorithm.
  JankRegion region_experimental_;

  // Tracks the short period after an input event during which we ignore jank.
  TaskRunnerTimer<JankTracker> timer_;

  // The maximum distance any layout object has moved in the current animation
  // frame.
  float frame_max_distance_;

  // The maximum distance any layout object has moved, across all animation
  // frames.
  float overall_max_distance_;

  // Whether either a user input or document scroll have been observed.
  bool observed_input_or_scroll_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_JANK_TRACKER_H_
