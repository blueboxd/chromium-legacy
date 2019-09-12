// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_CONTROLLER_H_

#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"

namespace cc {
// This class is responsible for hit testing composited scrollbars, event
// handling and creating gesture scroll deltas.
class CC_EXPORT ScrollbarController {
 public:
  explicit ScrollbarController(LayerTreeHostImpl*);
  virtual ~ScrollbarController();

  InputHandlerPointerResult HandlePointerDown(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandleMouseMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandlePointerUp(
      const gfx::PointF position_in_widget);

  // "velocity" here is calculated based on the initial scroll delta (See
  // InitialDeltaToAutoscrollVelocity). This value carries a "sign" which is
  // needed to determine whether we should set up the autoscrolling in the
  // forwards or the backwards direction.
  void StartAutoScrollAnimation(float velocity,
                                ElementId element_id,
                                ScrollbarPart pressed_scrollbar_part);
  bool ScrollbarScrollIsActive() { return scrollbar_scroll_is_active_; }
  ScrollbarOrientation orientation() {
    return currently_captured_scrollbar_->orientation();
  }

  void WillBeginImplFrame();

 private:
  // "Autoscroll" here means the continuous scrolling that occurs when the
  // pointer is held down on a hit-testable area of the scrollbar such as an
  // arrows of the track itself.
  enum AutoScrollDirection { AUTOSCROLL_FORWARD, AUTOSCROLL_BACKWARD };

  struct CC_EXPORT AutoScrollState {
    // Can only be either AUTOSCROLL_FORWARD or AUTOSCROLL_BACKWARD.
    AutoScrollDirection direction = AutoScrollDirection::AUTOSCROLL_FORWARD;

    // Stores the autoscroll velocity. The sign is used to set the "direction".
    float velocity = 0.f;

    // Used to track the scroller length while autoscrolling. Helpful for
    // setting up infinite scrolling.
    float scroll_layer_length = 0.f;

    // Used to lookup the rect corresponding to the ScrollbarPart so that
    // autoscroll animations can be played/paused depending on the current
    // pointer location.
    ScrollbarPart pressed_scrollbar_part;
  };

  // Helper to convert scroll offset to autoscroll velocity.
  float InitialDeltaToAutoscrollVelocity(gfx::ScrollOffset scroll_offset) const;

  // Returns the hit tested ScrollbarPart based on the position_in_widget.
  ScrollbarPart GetScrollbarPartFromPointerDown(
      const gfx::PointF position_in_widget);

  // Returns scroll offsets based on which ScrollbarPart was hit tested.
  gfx::ScrollOffset GetScrollOffsetForScrollbarPart(
      const ScrollbarPart scrollbar_part,
      const ScrollbarOrientation orientation);

  // Returns the rect for the ScrollbarPart.
  gfx::Rect GetRectForScrollbarPart(const ScrollbarPart scrollbar_part);

  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget);
  int GetScrollDeltaForScrollbarPart(ScrollbarPart scrollbar_part);

  // Makes position_in_widget relative to the scrollbar.
  gfx::PointF GetScrollbarRelativePosition(const gfx::PointF position_in_widget,
                                           bool* clipped);

  // Decides whether a track autoscroll should be aborted (or restarted) due to
  // the thumb reaching the pointer or the pointer leaving (or re-entering) the
  // bounds.
  void RecomputeAutoscrollStateIfNeeded();

  // Calculates the scroll_offset based on position_in_widget and
  // drag_anchor_relative_to_thumb_.
  gfx::ScrollOffset GetScrollOffsetForDragPosition(
      const gfx::PointF pointer_position_in_widget);

  // Returns a Vector2dF for position_in_widget relative to the scrollbar thumb.
  gfx::Vector2dF GetThumbRelativePoint(const gfx::PointF position_in_widget);

  // Returns the ratio of the scroller length to the scrollbar length. This is
  // needed to scale the scroll delta for thumb drag.
  float GetScrollerToScrollbarRatio();
  LayerTreeHostImpl* layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;
  const ScrollbarLayerImplBase* currently_captured_scrollbar_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF last_known_pointer_position_;

  // Holds information pertaining to autoscrolling. This member is empty if and
  // only if an autoscroll is *not* in progress.
  base::Optional<AutoScrollState> autoscroll_state_;

  // This is used to track the pointer location relative to the thumb origin
  // when a drag has started. It is empty if a thumb drag is *not* in progress.
  base::Optional<gfx::Vector2dF> drag_anchor_relative_to_thumb_;

  // Used to track if a GSU was processed for the current frame or not. Without
  // this, thumb drag will appear jittery. The reason this happens is because
  // when the first GSU is processed, it gets queued in the compositor thread
  // event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched and
  // pointermoves are not VSync aligned).
  bool drag_processed_for_current_frame_;

  std::unique_ptr<base::CancelableClosure> cancelable_autoscroll_task_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
