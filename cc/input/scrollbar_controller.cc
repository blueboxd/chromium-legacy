// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "base/cancelable_callback.h"
#include "cc/base/math_util.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"

namespace cc {
ScrollbarController::~ScrollbarController() {
  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }
}

ScrollbarController::ScrollbarController(
    LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      scrollbar_scroll_is_active_(false),
      thumb_drag_in_progress_(false),
      autoscroll_state_(AutoScrollState::NO_AUTOSCROLL),
      currently_captured_scrollbar_(nullptr),
      previous_pointer_position_(gfx::PointF(0, 0)),
      cancelable_autoscroll_task_(nullptr) {}

void ScrollbarController::WillBeginImplFrame() {
  // TODO(arakeri): Revisit this when addressing crbug.com/967004. The
  // animations need to be aborted/restarted based on the pointer location (i.e
  // leaving/entering the track/arrows, reaching the track end etc). The
  // autoscroll_state_ however, needs to be reset on pointer changes.
  if (autoscroll_state_ != AutoScrollState::NO_AUTOSCROLL &&
      ShouldCancelTrackAutoscroll())
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();
}

// Performs hit test and prepares scroll deltas that will be used by GSB and
// GSU.
InputHandlerPointerResult ScrollbarController::HandleMouseDown(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  LayerImpl* layer_impl = GetLayerHitByPoint(position_in_widget);

  // If a non-custom scrollbar layer was not found, we return early as there is
  // no point in setting additional state in the ScrollbarController.
  if (!(layer_impl && layer_impl->ToScrollbarLayer()))
    return scroll_result;

  currently_captured_scrollbar_ = layer_impl->ToScrollbarLayer();
  scroll_result.type = PointerResultType::kScrollbarScroll;
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  ScrollbarPart scrollbar_part =
      GetScrollbarPartFromPointerDown(position_in_widget);
  scroll_result.scroll_offset = GetScrollOffsetForScrollbarPart(
      scrollbar_part, currently_captured_scrollbar_->orientation());
  previous_pointer_position_ = position_in_widget;
  scrollbar_scroll_is_active_ = true;
  if (scrollbar_part == ScrollbarPart::THUMB) {
    thumb_drag_in_progress_ = true;
    scroll_result.scroll_units =
        ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  } else {
    // TODO(arakeri): This needs to be updated to kLine once cc implements
    // handling it. crbug.com/959441
    scroll_result.scroll_units =
        ui::input_types::ScrollGranularity::kScrollByPixel;
  }

  // Thumb drag is the only scrollbar manipulation that cannot produce an
  // autoscroll. All other interactions like clicking on arrows/trackparts have
  // the potential of initiating an autoscroll (if held down long enough).
  if (!scroll_result.scroll_offset.IsZero() && !thumb_drag_in_progress_) {
    cancelable_autoscroll_task_ = std::make_unique<base::CancelableClosure>(
        base::Bind(&ScrollbarController::StartAutoScrollAnimation,
                   base::Unretained(this), scroll_result.scroll_offset,
                   currently_captured_scrollbar_->scroll_element_id()));
    layer_tree_host_impl_->task_runner_provider()
        ->ImplThreadTaskRunner()
        ->PostDelayedTask(FROM_HERE, cancelable_autoscroll_task_->callback(),
                          kInitialAutoscrollTimerDelay);
  }
  return scroll_result;
}

// Performs hit test and prepares scroll deltas that will be used by GSU.
InputHandlerPointerResult ScrollbarController::HandleMouseMove(
    const gfx::PointF position_in_widget) {
  const gfx::PointF previous_pointer_position = previous_pointer_position_;
  previous_pointer_position_ = position_in_widget;
  InputHandlerPointerResult scroll_result;
  if (!thumb_drag_in_progress_)
    return scroll_result;

  scroll_result.type = PointerResultType::kScrollbarScroll;
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  if (orientation == ScrollbarOrientation::VERTICAL)
    scroll_result.scroll_offset.set_y(position_in_widget.y() -
                                      previous_pointer_position.y());
  else
    scroll_result.scroll_offset.set_x(position_in_widget.x() -
                                      previous_pointer_position.x());

  LayerImpl* owner_scroll_layer =
      layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
          currently_captured_scrollbar_->scroll_element_id());

  // Calculating the delta by which the scroller layer should move when
  // dragging the thumb depends on the following factors:
  // - scrollbar_track_length
  // - scrollbar_thumb_length
  // - scroll_layer_length
  // - viewport_length
  // - device_scale_factor
  // - position_in_widget
  //
  // When a thumb drag is in progress, for every pixel that the pointer moves,
  // the delta for the corresponding scroll_layer needs to be scaled by the
  // following ratio:
  // scaled_scroller_to_scrollbar_ratio =
  //  (scroll_layer_length - viewport_length) /
  //   (scrollbar_track_length - scrollbar_thumb_length)
  //
  // |<--------------------- scroll_layer_length -------------------------->|
  //
  // +------------------------------------------------+......................
  // |                                                |                     .
  // |<-------------- viewport_length --------------->|                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |  |<------- scrollbar_track_length --------->|  |                     .
  // |                                                |                     .
  // +--+-----+----------------------------+-------+--+......................
  // |<||     |############################|       ||>|
  // +--+-----+----------------------------+-------+--+
  //
  //          |<- scrollbar_thumb_length ->|
  //
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  float scroll_layer_length =
      currently_captured_scrollbar_->scroll_layer_length();
  float scrollbar_track_length = currently_captured_scrollbar_->TrackLength();
  gfx::Rect thumb_rect(currently_captured_scrollbar_->ComputeThumbQuadRect());
  float scrollbar_thumb_length = orientation == ScrollbarOrientation::VERTICAL
                                     ? thumb_rect.height()
                                     : thumb_rect.width();

  float viewport_length =
      orientation == ScrollbarOrientation::VERTICAL
          ? owner_scroll_layer->scroll_container_bounds().height()
          : (owner_scroll_layer->scroll_container_bounds().width());
  float scaled_scroller_to_scrollbar_ratio =
      ((scroll_layer_length - viewport_length) /
       (scrollbar_track_length - scrollbar_thumb_length)) *
      layer_tree_host_impl_->active_tree()->device_scale_factor();
  scroll_result.scroll_offset.Scale(scaled_scroller_to_scrollbar_ratio);
  // Thumb drags have more granularity and are purely dependent on the pointer
  // movement. Hence we use kPrecisePixel when dragging the thumb.
  scroll_result.scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;

  return scroll_result;
}

bool ScrollbarController::ShouldCancelTrackAutoscroll() {
  // Should only ever be called if an autoscroll is in progress.
  DCHECK(autoscroll_state_ != AutoScrollState::NO_AUTOSCROLL);

  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  const gfx::Rect thumb_quad =
      currently_captured_scrollbar_->ComputeThumbQuadRect();

  bool clipped;
  gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(previous_pointer_position_, &clipped));

  if (clipped)
    return false;

  // Based on the orientation of the scrollbar and the direction of the
  // autoscroll, the code below makes a decision of whether the track autoscroll
  // should be canceled or not.
  int thumb_start = 0;
  int thumb_end = 0;
  int pointer_position = 0;
  if (orientation == ScrollbarOrientation::VERTICAL) {
    thumb_start = thumb_quad.y();
    thumb_end = thumb_quad.y() + thumb_quad.height();
    pointer_position = scroller_relative_position.y();
  } else {
    thumb_start = thumb_quad.x();
    thumb_end = thumb_quad.x() + thumb_quad.width();
    pointer_position = scroller_relative_position.x();
  }

  if ((autoscroll_state_ == AutoScrollState::AUTOSCROLL_FORWARD &&
       thumb_end > pointer_position) ||
      (autoscroll_state_ == AutoScrollState::AUTOSCROLL_BACKWARD &&
       thumb_start < pointer_position))
    return true;

  return false;
}

void ScrollbarController::StartAutoScrollAnimation(
    gfx::ScrollOffset scroll_offset,
    ElementId element_id) {
  // scroll_node is set up while handling GSB. If there's no node to scroll, we
  // don't need to create any animation for it.
  ScrollTree& scroll_tree =
      layer_tree_host_impl_->active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);

  if (!(scroll_node && scrollbar_scroll_is_active_))
    return;

  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  // TODO(arakeri): The animation needs to be readjusted if the scroller length
  // changes. Tracked here: crbug.com/972485
  float scroll_layer_length =
      currently_captured_scrollbar_->scroll_layer_length();

  gfx::ScrollOffset current_offset =
      scroll_tree.current_scroll_offset(scroll_node->element_id);
  gfx::Vector2dF target_offset;

  // Determine the max offset for the scroll based on the scrolling direction.
  // Negative scroll_delta indicates backwards scrolling whereas a positive
  // scroll_delta indicates forwards scrolling.
  float scroll_delta = 0;
  if (orientation == ScrollbarOrientation::VERTICAL) {
    DCHECK_NE(scroll_offset.y(), 0);
    scroll_delta = scroll_offset.y();
    float final_offset = scroll_delta < 0 ? 0 : scroll_layer_length;
    target_offset = gfx::Vector2dF(current_offset.x(), final_offset);
  } else {
    DCHECK_NE(scroll_offset.x(), 0);
    scroll_delta = scroll_offset.x();
    float final_offset = scroll_delta < 0 ? 0 : scroll_layer_length;
    target_offset = gfx::Vector2dF(final_offset, current_offset.y());
  }

  autoscroll_state_ = scroll_delta < 0 ? AutoScrollState::AUTOSCROLL_BACKWARD
                                       : AutoScrollState::AUTOSCROLL_FORWARD;
  float autoscroll_velocity = std::abs(scroll_delta) * kAutoscrollMultiplier;
  layer_tree_host_impl_->AutoScrollAnimationCreate(scroll_node, target_offset,
                                                   autoscroll_velocity);
}

// Performs hit test and prepares scroll deltas that will be used by GSE.
InputHandlerPointerResult ScrollbarController::HandleMouseUp(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (scrollbar_scroll_is_active_) {
    scrollbar_scroll_is_active_ = false;
    scroll_result.type = PointerResultType::kScrollbarScroll;
  }

  // TODO(arakeri): This needs to be moved to ScrollOffsetAnimationsImpl as it
  // has knowledge about what type of animation is running. crbug.com/976353
  // Only abort the animation if it is an "autoscroll" animation.
  if (autoscroll_state_ != AutoScrollState::NO_AUTOSCROLL)
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();

  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }

  thumb_drag_in_progress_ = false;
  autoscroll_state_ = AutoScrollState::NO_AUTOSCROLL;
  return scroll_result;
}

// Returns the layer that is hit by the position_in_widget.
LayerImpl* ScrollbarController::GetLayerHitByPoint(
    const gfx::PointF position_in_widget) {
  LayerTreeImpl* active_tree = layer_tree_host_impl_->active_tree();
  gfx::Point viewport_point(position_in_widget.x(), position_in_widget.y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
          device_viewport_point);

  return layer_impl;
}

int ScrollbarController::GetScrollDeltaForScrollbarPart(
    ScrollbarPart scrollbar_part) {
  int scroll_delta = 0;
  int viewport_length = 0;
  LayerImpl* owner_scroll_layer = nullptr;

  switch (scrollbar_part) {
    case ScrollbarPart::BACK_BUTTON:
    case ScrollbarPart::FORWARD_BUTTON:
      scroll_delta = kPixelsPerLineStep;
      break;
    case ScrollbarPart::BACK_TRACK:
    case ScrollbarPart::FORWARD_TRACK:
      owner_scroll_layer =
          layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
              currently_captured_scrollbar_->scroll_element_id());
      viewport_length =
          currently_captured_scrollbar_->orientation() ==
                  ScrollbarOrientation::VERTICAL
              ? owner_scroll_layer->scroll_container_bounds().height()
              : (owner_scroll_layer->scroll_container_bounds().width());
      scroll_delta = viewport_length * kMinFractionToStepWhenPaging;
      break;
    default:
      scroll_delta = 0;
  }
  return scroll_delta *
         layer_tree_host_impl_->active_tree()->device_scale_factor();
}

gfx::PointF ScrollbarController::GetScrollbarRelativePosition(
    const gfx::PointF position_in_widget,
    bool* clipped) {
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!currently_captured_scrollbar_->ScreenSpaceTransform().GetInverse(
          &inverse_screen_space_transform))
    return gfx::PointF(0, 0);

  return gfx::PointF(MathUtil::ProjectPoint(inverse_screen_space_transform,
                                            position_in_widget, clipped));
}

// Determines the ScrollbarPart based on the position_in_widget.
ScrollbarPart ScrollbarController::GetScrollbarPartFromPointerDown(
    const gfx::PointF position_in_widget) {
  // position_in_widget needs to be transformed and made relative to the
  // scrollbar layer because hit testing assumes layer relative coordinates.
  bool clipped = false;

  const gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(position_in_widget, &clipped));

  if (clipped)
    return ScrollbarPart::NO_PART;

  return currently_captured_scrollbar_->IdentifyScrollbarPart(
      scroller_relative_position);
}

// Determines the scroll offsets based on the ScrollbarPart and the scrollbar
// orientation.
gfx::ScrollOffset ScrollbarController::GetScrollOffsetForScrollbarPart(
    const ScrollbarPart scrollbar_part,
    const ScrollbarOrientation orientation) {
  float scroll_delta = GetScrollDeltaForScrollbarPart(scrollbar_part);

  // See CreateScrollStateForGesture for more information on how these values
  // will be interpreted.
  if (scrollbar_part == ScrollbarPart::BACK_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Up arrow
               : gfx::ScrollOffset(-scroll_delta, 0);  // Left arrow
  } else if (scrollbar_part == ScrollbarPart::FORWARD_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Down arrow
               : gfx::ScrollOffset(scroll_delta, 0);  // Right arrow
  } else if (scrollbar_part == ScrollbarPart::BACK_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Track click up
               : gfx::ScrollOffset(-scroll_delta, 0);  // Track click left
  } else if (scrollbar_part == ScrollbarPart::FORWARD_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Track click down
               : gfx::ScrollOffset(scroll_delta, 0);  // Track click right
  }

  return gfx::ScrollOffset(0, 0);
}

}  // namespace cc
