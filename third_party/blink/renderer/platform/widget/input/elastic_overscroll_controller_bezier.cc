// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {
// The following constants are determined experimentally.

// Used to determine how far the scroller is allowed to stretch.
constexpr double kOverscrollBoundaryMultiplier = 0.1f;

// Maximum duration for the bounce back animation.
constexpr double kBounceBackMaxDurationMilliseconds = 300.0;

// Time taken by the bounce back animation (in milliseconds) to scroll 1 px.
constexpr double kBounceBackMillisecondsPerPixel = 15.0;

// Control points for the Cubic Bezier curve.
constexpr double kBounceBackwardsX1 = 0.05;
constexpr double kBounceBackwardsY1 = 0.7;
constexpr double kBounceBackwardsX2 = 0.25;
constexpr double kBounceBackwardsY2 = 1.0;

base::TimeDelta CalculateBounceBackDuration(double bounce_back_distance) {
  return base::TimeDelta::FromMillisecondsD(std::min(
      kBounceBackMaxDurationMilliseconds,
      kBounceBackMillisecondsPerPixel * std::abs(bounce_back_distance)));
}
}  // namespace

ElasticOverscrollControllerBezier::ElasticOverscrollControllerBezier(
    cc::ScrollElasticityHelper* helper)
    : ElasticOverscrollController(helper),
      bounce_backwards_curve_(gfx::CubicBezier(kBounceBackwardsX1,
                                               kBounceBackwardsY1,
                                               kBounceBackwardsX2,
                                               kBounceBackwardsY2)) {}

// Returns the maximum amount to be overscrolled.
gfx::Vector2dF ElasticOverscrollControllerBezier::OverscrollBoundary(
    const gfx::Size& scroller_bounds) const {
  return gfx::Vector2dF(
      scroller_bounds.width() * kOverscrollBoundaryMultiplier,
      scroller_bounds.height() * kOverscrollBoundaryMultiplier);
}

void ElasticOverscrollControllerBezier::DidEnterMomentumAnimatedState() {
  bounce_backwards_duration_x_ =
      CalculateBounceBackDuration(momentum_animation_initial_stretch_.x());
  bounce_backwards_duration_y_ =
      CalculateBounceBackDuration(momentum_animation_initial_stretch_.y());
}

gfx::Vector2d ElasticOverscrollControllerBezier::StretchAmountForTimeDelta(
    const base::TimeDelta& delta) const {
  // Handle the case where the animation is in the bounce-back stage.
  double stretch_x, stretch_y;
  stretch_x = stretch_y = 0.f;
  if (delta < bounce_backwards_duration_x_) {
    double curve_progress = delta.InMillisecondsF() /
                            bounce_backwards_duration_x_.InMillisecondsF();
    double progress = bounce_backwards_curve_.Solve(curve_progress);
    stretch_x = momentum_animation_initial_stretch_.x() * (1 - progress);
  }

  if (delta < bounce_backwards_duration_y_) {
    double curve_progress = delta.InMillisecondsF() /
                            bounce_backwards_duration_y_.InMillisecondsF();
    double progress = bounce_backwards_curve_.Solve(curve_progress);
    stretch_y = momentum_animation_initial_stretch_.y() * (1 - progress);
  }

  return gfx::ToRoundedVector2d(gfx::Vector2dF(stretch_x, stretch_y));
}

// The goal of this calculation is to map the distance the user has scrolled
// past the boundary into the distance to actually scroll the elastic scroller.
gfx::Vector2d
ElasticOverscrollControllerBezier::StretchAmountForAccumulatedOverscroll(
    const gfx::Vector2dF& accumulated_overscroll) const {
  // TODO(arakeri): This should change as you pinch zoom in.
  const gfx::Size& scroller_bounds = GetScrollBounds();
  const gfx::Vector2dF overscroll_boundary =
      OverscrollBoundary(scroller_bounds);

  // We use the tanh function in addition to the mapping, which gives it more of
  // a spring effect. However, we want to use tanh's range from [0, 2], so we
  // multiply the value we provide to tanh by 2.

  // Also, it may happen that the scroller_bounds are 0 if the viewport scroll
  // nodes are null (see: ScrollElasticityHelper::ScrollBounds). We therefore
  // have to check in order to avoid a divide by 0.
  gfx::Vector2d overbounce_distance;
  if (scroller_bounds.width() > 0.f) {
    overbounce_distance.set_x(
        tanh(2 * accumulated_overscroll.x() / scroller_bounds.width()) *
        overscroll_boundary.x());
  }

  if (scroller_bounds.height() > 0.f) {
    overbounce_distance.set_y(
        tanh(2 * accumulated_overscroll.y() / scroller_bounds.height()) *
        overscroll_boundary.y());
  }

  return overbounce_distance;
}

// This function does the inverse of StretchAmountForAccumulatedOverscroll. As
// in, instead of taking in the amount of distance overscrolled to get the
// bounce distance, it takes in the bounce distance and calculates how much is
// actually overscrolled.
gfx::Vector2d
ElasticOverscrollControllerBezier::AccumulatedOverscrollForStretchAmount(
    const gfx::Vector2dF& stretch_amount) const {
  const gfx::Size& scroller_bounds = GetScrollBounds();
  const gfx::Vector2dF overscroll_boundary =
      OverscrollBoundary(scroller_bounds);

  // It may happen that the scroller_bounds are 0 if the viewport scroll
  // nodes are null (see: ScrollElasticityHelper::ScrollBounds). We therefore
  // have to check in order to avoid a divide by 0.
  gfx::Vector2d overscrolled_amount;
  if (overscroll_boundary.x() > 0.f) {
    float atanh_value = atanh(stretch_amount.x() / overscroll_boundary.x());
    overscrolled_amount.set_x((atanh_value / 2) * scroller_bounds.width());
  }

  if (overscroll_boundary.y() > 0.f) {
    float atanh_value = atanh(stretch_amount.y() / overscroll_boundary.y());
    overscrolled_amount.set_y((atanh_value / 2) * scroller_bounds.height());
  }

  return overscrolled_amount;
}
}  // namespace blink
