// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_TIMELINE_H_
#define CC_ANIMATION_SCROLL_TIMELINE_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"

namespace cc {

class ScrollTree;

// A ScrollTimeline is an animation timeline that bases its current time on the
// progress of scrolling in some scroll container.
//
// This is the compositor-side representation of the web concept expressed in
// https://wicg.github.io/scroll-animations/#scrolltimeline-interface.
class CC_ANIMATION_EXPORT ScrollTimeline : public AnimationTimeline {
 public:
  // cc does not know about writing modes. The ScrollDirection below is
  // converted using blink::scroll_timeline_util::ConvertOrientation which takes
  // the spec-compliant ScrollDirection enumeration.
  // https://drafts.csswg.org/scroll-animations/#scrolldirection-enumeration
  enum ScrollDirection {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
  };

  ScrollTimeline(base::Optional<ElementId> scroller_id,
                 ScrollDirection direction,
                 base::Optional<double> start_scroll_offset,
                 base::Optional<double> end_scroll_offset,
                 double time_range,
                 int animation_timeline_id);

  static scoped_refptr<ScrollTimeline> Create(
      base::Optional<ElementId> scroller_id,
      ScrollDirection direction,
      base::Optional<double> start_scroll_offset,
      base::Optional<double> end_scroll_offset,
      double time_range);

  // Create a copy of this ScrollTimeline intended for the impl thread in the
  // compositor.
  scoped_refptr<AnimationTimeline> CreateImplInstance() const override;

  // ScrollTimeline is active if the scroll node exists in active or pending
  // scroll tree.
  virtual bool IsActive(const ScrollTree& scroll_tree,
                        bool is_active_tree) const;

  // Calculate the current time of the ScrollTimeline. This is either a
  // base::TimeTicks value or base::nullopt if the current time is unresolved.
  // The internal calculations are performed using doubles and the result is
  // converted to base::TimeTicks. This limits the precision to 1us.
  virtual base::Optional<base::TimeTicks> CurrentTime(
      const ScrollTree& scroll_tree,
      bool is_active_tree) const;

  void UpdateScrollerIdAndScrollOffsets(
      base::Optional<ElementId> scroller_id,
      base::Optional<double> start_scroll_offset,
      base::Optional<double> end_scroll_offset);

  void PushPropertiesTo(AnimationTimeline* impl_timeline) override;
  void ActivateTimeline() override;

  base::Optional<ElementId> GetActiveIdForTest() const { return active_id_; }
  base::Optional<ElementId> GetPendingIdForTest() const { return pending_id_; }
  ScrollDirection GetDirectionForTest() const { return direction_; }
  base::Optional<double> GetStartScrollOffsetForTest() const {
    return start_scroll_offset_;
  }
  base::Optional<double> GetEndScrollOffsetForTest() const {
    return end_scroll_offset_;
  }
  double GetTimeRangeForTest() const { return time_range_; }

  bool IsScrollTimeline() const override;

 protected:
  ~ScrollTimeline() override;

 private:
  // The scroller which this ScrollTimeline is based on. The same underlying
  // scroll source may have different ids in the pending and active tree (see
  // http://crbug.com/847588).
  base::Optional<ElementId> active_id_;
  base::Optional<ElementId> pending_id_;

  // The direction of the ScrollTimeline indicates which axis of the scroller
  // it should base its current time on, and where the origin point is.
  ScrollDirection direction_;

  // These define the total range of the scroller that the ScrollTimeline is
  // active within. If not set they default to the beginning/end of the scroller
  // respectively, respecting the current |direction_|.
  base::Optional<double> start_scroll_offset_;
  base::Optional<double> end_scroll_offset_;

  // A ScrollTimeline maps from the scroll offset in the scroller to a time
  // value based on a 'time range'. See the implementation of CurrentTime or the
  // spec for details.
  double time_range_;
};

inline ScrollTimeline* ToScrollTimeline(AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<ScrollTimeline*>(timeline);
}

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
