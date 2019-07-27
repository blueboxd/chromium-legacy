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

  InputHandlerPointerResult HandleMouseDown(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandleMouseMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandleMouseUp(const gfx::PointF position_in_widget);

  // scroll_offset is the delta from the initial click. This is needed to
  // determine whether we should set up the autoscrolling in the forwards or the
  // backwards direction and the speed of the animation.
  void StartAutoScrollAnimation(gfx::ScrollOffset scroll_offset,
                                ElementId element_id);
  bool ScrollbarScrollIsActive() { return scrollbar_scroll_is_active_; }
  ScrollbarOrientation orientation() {
    return currently_captured_scrollbar_->orientation();
  }

  void WillBeginImplFrame();

 private:
  // Returns the hit tested ScrollbarPart based on the position_in_widget.
  ScrollbarPart GetScrollbarPartFromPointerDown(
      const gfx::PointF position_in_widget);

  // Returns scroll offsets based on which ScrollbarPart was hit tested.
  gfx::ScrollOffset GetScrollOffsetForScrollbarPart(
      const ScrollbarPart scrollbar_part,
      const ScrollbarOrientation orientation);

  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget);
  int GetScrollDeltaForScrollbarPart(ScrollbarPart scrollbar_part);

  // Makes position_in_widget relative to the scrollbar.
  gfx::PointF GetScrollbarRelativePosition(const gfx::PointF position_in_widget,
                                           bool* clipped);

  // Decides whether a track autoscroll should be aborted.
  bool ShouldCancelTrackAutoscroll();

  LayerTreeHostImpl* layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;

  // Used to tell if the scrollbar thumb is getting dragged.
  bool thumb_drag_in_progress_;

  // "Autoscroll" here means the continuous scrolling that occurs when the
  // pointer is held down on a hit-testable area of the scrollbar such as an
  // arrows of the track itself.
  AutoScrollState autoscroll_state_;
  const ScrollbarLayerImplBase* currently_captured_scrollbar_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF previous_pointer_position_;

  std::unique_ptr<base::CancelableClosure> cancelable_autoscroll_task_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
