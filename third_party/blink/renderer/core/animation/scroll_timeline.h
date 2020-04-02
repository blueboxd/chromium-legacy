// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DoubleOrScrollTimelineAutoKeyword;
class ScrollTimelineOptions;

// Implements the ScrollTimeline concept from the Scroll-linked Animations spec.
//
// A ScrollTimeline is a special form of AnimationTimeline whose time values are
// not determined by wall-clock time but instead the progress of scrolling in a
// scroll container. The user is able to specify which scroll container to
// track, the direction of scroll they care about, and various attributes to
// control the conversion of scroll amount to time output.
//
// Spec: https://wicg.github.io/scroll-animations/#scroll-timelines
class CORE_EXPORT ScrollTimeline : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum ScrollDirection {
    Block,
    Inline,
    Horizontal,
    Vertical,
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions*,
                                ExceptionState&);

  ScrollTimeline(Document*,
                 Element*,
                 ScrollDirection,
                 CSSPrimitiveValue*,
                 CSSPrimitiveValue*,
                 double);

  // AnimationTimeline implementation.
  bool IsScrollTimeline() const override { return true; }
  // ScrollTimeline is not active if scrollSource is null, does not currently
  // have a CSS layout box, or if its layout box is not a scroll container.
  // https://github.com/WICG/scroll-animations/issues/31
  bool IsActive() const override;
  base::Optional<base::TimeDelta> InitialStartTimeForAnimations() override;

  void ServiceAnimations(TimingUpdateReason) override;
  void ScheduleNextService() override;

  // IDL API implementation.
  Element* scrollSource();
  String orientation();
  String startScrollOffset();
  String endScrollOffset();
  void timeRange(DoubleOrScrollTimelineAutoKeyword&);

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |scrollSource| when |scroll_source_| is the
  // Document's scrollingElement, and it may be null if the document was removed
  // before the ScrollTimeline was created.
  Node* ResolvedScrollSource() const { return resolved_scroll_source_; }

  ScrollDirection GetOrientation() const { return orientation_; }

  void GetCurrentAndMaxOffset(const LayoutBox*,
                              double& current_offset,
                              double& max_offset) const;
  void ResolveScrollStartAndEnd(const LayoutBox*,
                                double max_offset,
                                double& resolved_start_scroll_offset,
                                double& resolved_end_scroll_offset) const;
  // Invalidates scroll timeline as a result of scroller properties change.
  // This may lead the timeline to request a new animation frame.
  virtual void Invalidate();

  CompositorAnimationTimeline* EnsureCompositorTimeline() override;
  // TODO(crbug.com/896249): These methods are temporary and currently required
  // to support worklet animations. Once worklet animations become animations
  // these methods will not be longer needed. They are used to keep track of
  // number of worklet animations attached to the scroll timeline for updating
  // compositing state.
  void WorkletAnimationAttached();
  void WorkletAnimationDetached();

  void Trace(Visitor*) override;

  static bool HasActiveScrollTimeline(Node* node);
  // Invalidates scroll timelines with a given scroller node.
  // Called when scroller properties, affecting scroll timeline state, change.
  // These properties are scroller offset, content size, viewport size,
  // overflow, adding and removal of scrollable area.
  static void Invalidate(Node* node);

 protected:
  PhaseAndTime CurrentPhaseAndTime() override;

 private:
  // https://wicg.github.io/scroll-animations/#avoiding-cycles
  // Snapshots scroll timeline current time and phase.
  // Called once per animation frame.
  void SnapshotState();
  bool ComputeIsActive() const;
  PhaseAndTime ComputeCurrentPhaseAndTime() const;

  // Use |scroll_source_| only to implement the web-exposed API but use
  // resolved_scroll_source_ to actually access the scroll related properties.
  Member<Element> scroll_source_;
  Member<Node> resolved_scroll_source_;
  ScrollDirection orientation_;
  Member<CSSPrimitiveValue> start_scroll_offset_;
  Member<CSSPrimitiveValue> end_scroll_offset_;
  double time_range_;

  // Snapshotted value produced by the last SnapshotState call.
  PhaseAndTime phase_and_time_snapshotted_;
};

template <>
struct DowncastTraits<ScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsScrollTimeline();
  }
};

}  // namespace blink

#endif
