// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/overscroll_bounce_controller.h"

#include <algorithm>
#include <cmath>

// TODO(arakeri): This is where all the overscroll specific code will go.
namespace blink {

constexpr float kOverscrollBoundaryMultiplier = 0.1f;

OverscrollBounceController::OverscrollBounceController(
    cc::ScrollElasticityHelper* helper)
    : state_(kStateInactive), helper_(helper), weak_factory_(this) {}

OverscrollBounceController::~OverscrollBounceController() = default;

base::WeakPtr<ElasticOverscrollController>
OverscrollBounceController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OverscrollBounceController::Animate(base::TimeTicks time) {}

// TODO(arakeri): ReconcileStretchAndScroll implementations in both the classes
// InputScrollElasticityController and  OverscrollBounceController have common
// code that needs to be evaluated and moved up into the base class.
void OverscrollBounceController::ReconcileStretchAndScroll() {
  const gfx::Vector2dF stretch = helper_->StretchAmount();
  if (stretch.IsZero())
    return;

  const gfx::ScrollOffset scroll_offset = helper_->ScrollOffset();
  const gfx::ScrollOffset max_scroll_offset = helper_->MaxScrollOffset();

  float scroll_adjustment_x = 0;
  if (stretch.x() < 0.f)
    scroll_adjustment_x = scroll_offset.x();
  else if (stretch.x() > 0.f)
    scroll_adjustment_x = max_scroll_offset.x() - scroll_offset.x();

  float scroll_adjustment_y = 0;
  if (stretch.y() < 0.f)
    scroll_adjustment_y = scroll_offset.y();
  else if (stretch.y() > 0.f)
    scroll_adjustment_y = max_scroll_offset.y() - scroll_offset.y();

  if (state_ == kStateActiveScroll) {
    // During an active scroll, we want to reduce |accumulated_scroll_delta_| by
    // the amount that was scrolled (but we don't want to over-consume, so limit
    // it by the amount of |accumulated_scroll_delta_|).
    scroll_adjustment_x = std::copysign(
        std::min(std::abs(accumulated_scroll_delta_.x()), scroll_adjustment_x),
        stretch.x());
    scroll_adjustment_y = std::copysign(
        std::min(std::abs(accumulated_scroll_delta_.y()), scroll_adjustment_y),
        stretch.y());

    accumulated_scroll_delta_ -=
        gfx::Vector2dF(scroll_adjustment_x, scroll_adjustment_y);
    helper_->SetStretchAmount(OverscrollBounceDistance(
        accumulated_scroll_delta_, helper_->ScrollBounds()));
  }

  helper_->ScrollBy(gfx::Vector2dF(scroll_adjustment_x, scroll_adjustment_y));
}

// Returns the maximum amount to be overscrolled.
gfx::Vector2dF OverscrollBounceController::OverscrollBoundary(
    const gfx::Size& scroller_bounds) const {
  return gfx::Vector2dF(
      scroller_bounds.width() * kOverscrollBoundaryMultiplier,
      scroller_bounds.height() * kOverscrollBoundaryMultiplier);
}

// The goal of this calculation is to map the distance the user has scrolled
// past the boundary into the distance to actually scroll the elastic scroller.
gfx::Vector2d OverscrollBounceController::OverscrollBounceDistance(
    const gfx::Vector2dF& distance_overscrolled,
    const gfx::Size& scroller_bounds) const {
  // TODO(arakeri): This should change as you pinch zoom in.
  gfx::Vector2dF overscroll_boundary = OverscrollBoundary(scroller_bounds);

  // We use the tanh function in addition to the mapping, which gives it more of
  // a spring effect. However, we want to use tanh's range from [0, 2], so we
  // multiply the value we provide to tanh by 2.

  // Also, it may happen that the scroller_bounds are 0 if the viewport scroll
  // nodes are null (see: ScrollElasticityHelper::ScrollBounds). We therefore
  // have to check in order to avoid a divide by 0.
  gfx::Vector2d overbounce_distance;
  if (scroller_bounds.width() > 0.f) {
    overbounce_distance.set_x(
        tanh(2 * distance_overscrolled.x() / scroller_bounds.width()) *
        overscroll_boundary.x());
  }

  if (scroller_bounds.height() > 0.f) {
    overbounce_distance.set_y(
        tanh(2 * distance_overscrolled.y() / scroller_bounds.height()) *
        overscroll_boundary.y());
  }

  return overbounce_distance;
}

void OverscrollBounceController::EnterStateActiveScroll() {
  state_ = kStateActiveScroll;
}

void OverscrollBounceController::ObserveRealScrollBegin(
    const blink::WebGestureEvent& gesture_event) {
  if (gesture_event.data.scroll_begin.inertial_phase ==
          blink::WebGestureEvent::InertialPhaseState::kNonMomentum &&
      gesture_event.data.scroll_begin.delta_hint_units ==
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    EnterStateActiveScroll();
  }
}

void OverscrollBounceController::ObserveRealScrollEnd() {
  state_ = kStateInactive;
}

void OverscrollBounceController::OverscrollIfNecessary(
    const gfx::Vector2dF& overscroll_delta) {
  accumulated_scroll_delta_ += overscroll_delta;
  gfx::Vector2d overbounce_distance = OverscrollBounceDistance(
      accumulated_scroll_delta_, helper_->ScrollBounds());
  helper_->SetStretchAmount(overbounce_distance);
}

void OverscrollBounceController::ObserveScrollUpdate(
    const gfx::Vector2dF& unused_scroll_delta) {
  if (state_ == kStateInactive)
    return;

  if (state_ == kStateActiveScroll) {
    // TODO(arakeri): Implement animate back.
    OverscrollIfNecessary(unused_scroll_delta);
  }
}

void OverscrollBounceController::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  switch (gesture_event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (gesture_event.data.scroll_begin.synthetic)
        return;
      ObserveRealScrollBegin(gesture_event);
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollUpdate: {
      gfx::Vector2dF event_delta(-gesture_event.data.scroll_update.delta_x,
                                 -gesture_event.data.scroll_update.delta_y);
      ObserveScrollUpdate(scroll_result.unused_scroll_delta);
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      ObserveRealScrollEnd();
      break;
    }
    default:
      break;
  }
}
}  // namespace blink
