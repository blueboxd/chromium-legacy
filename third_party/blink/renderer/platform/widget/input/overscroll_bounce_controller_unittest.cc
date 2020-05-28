// Copyright 2019 The Chromium Authors. All rights reserved.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a fork of the input_scroll_elasticity_controller_unittest.cc.

#include "third_party/blink/renderer/platform/widget/input/overscroll_bounce_controller.h"

#include "cc/input/input_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace blink {
namespace test {

class MockScrollElasticityHelper : public cc::ScrollElasticityHelper {
 public:
  MockScrollElasticityHelper() = default;
  ~MockScrollElasticityHelper() override = default;

  // cc::ScrollElasticityHelper implementation:
  gfx::Size ScrollBounds() const override { return gfx::Size(1000, 1000); }
  bool IsUserScrollable() const override { return false; }
  gfx::Vector2dF StretchAmount() const override { return stretch_amount_; }
  void SetStretchAmount(const gfx::Vector2dF& stretch_amount) override {
    stretch_amount_ = stretch_amount;
  }
  void ScrollBy(const gfx::Vector2dF& delta) override {
    scroll_offset_ += gfx::ScrollOffset(delta);
  }
  void RequestOneBeginFrame() override {}
  gfx::ScrollOffset ScrollOffset() const override { return scroll_offset_; }
  gfx::ScrollOffset MaxScrollOffset() const override {
    return max_scroll_offset_;
  }

  void SetScrollOffsetAndMaxScrollOffset(
      const gfx::ScrollOffset& scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset) {
    scroll_offset_ = scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
  }

 private:
  gfx::Vector2dF stretch_amount_;
  gfx::ScrollOffset scroll_offset_, max_scroll_offset_;
};

class OverscrollBounceControllerTest : public testing::Test {
 public:
  OverscrollBounceControllerTest() : controller_(&helper_) {}
  ~OverscrollBounceControllerTest() override = default;

  void SetUp() override {}

  void SendGestureScrollBegin(
      WebGestureEvent::InertialPhaseState inertialPhase) {
    WebGestureEvent event(WebInputEvent::Type::kGestureScrollBegin,
                          WebInputEvent::kNoModifiers, base::TimeTicks(),
                          WebGestureDevice::kTouchpad);
    event.data.scroll_begin.inertial_phase = inertialPhase;

    controller_.ObserveGestureEventAndResult(event,
                                             cc::InputHandlerScrollResult());
  }

  void SendGestureScrollUpdate(
      WebGestureEvent::InertialPhaseState inertialPhase,
      const gfx::Vector2dF& scroll_delta,
      const gfx::Vector2dF& unused_scroll_delta) {
    blink::WebGestureEvent event(WebInputEvent::Type::kGestureScrollUpdate,
                                 WebInputEvent::kNoModifiers, base::TimeTicks(),
                                 blink::WebGestureDevice::kTouchpad);
    event.data.scroll_update.inertial_phase = inertialPhase;
    event.data.scroll_update.delta_x = -scroll_delta.x();
    event.data.scroll_update.delta_y = -scroll_delta.y();

    cc::InputHandlerScrollResult scroll_result;
    scroll_result.did_overscroll_root = !unused_scroll_delta.IsZero();
    scroll_result.unused_scroll_delta = unused_scroll_delta;

    controller_.ObserveGestureEventAndResult(event, scroll_result);
  }
  void SendGestureScrollEnd() {
    WebGestureEvent event(WebInputEvent::Type::kGestureScrollEnd,
                          WebInputEvent::kNoModifiers, base::TimeTicks(),
                          WebGestureDevice::kTouchpad);

    controller_.ObserveGestureEventAndResult(event,
                                             cc::InputHandlerScrollResult());
  }

  MockScrollElasticityHelper helper_;
  OverscrollBounceController controller_;
};

// Tests the bounds of the overscroll and that the "StretchAmount" returns back
// to 0 once the overscroll is done.
TEST_F(OverscrollBounceControllerTest, VerifyOverscrollStretch) {
  // Test vertical overscroll.
  SendGestureScrollBegin(WebGestureEvent::InertialPhaseState::kNonMomentum);
  gfx::Vector2dF delta(0, -50);
  EXPECT_EQ(gfx::Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(WebGestureEvent::InertialPhaseState::kNonMomentum,
                          delta, gfx::Vector2dF(0, -100));
  EXPECT_EQ(gfx::Vector2dF(0, -19), helper_.StretchAmount());
  SendGestureScrollUpdate(WebGestureEvent::InertialPhaseState::kNonMomentum,
                          delta, gfx::Vector2dF(0, 100));
  EXPECT_EQ(gfx::Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollEnd();

  // Test horizontal overscroll.
  SendGestureScrollBegin(WebGestureEvent::InertialPhaseState::kNonMomentum);
  delta = gfx::Vector2dF(-50, 0);
  EXPECT_EQ(gfx::Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(WebGestureEvent::InertialPhaseState::kNonMomentum,
                          delta, gfx::Vector2dF(-100, 0));
  EXPECT_EQ(gfx::Vector2dF(-19, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(WebGestureEvent::InertialPhaseState::kNonMomentum,
                          delta, gfx::Vector2dF(100, 0));
  EXPECT_EQ(gfx::Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollEnd();
}

// Verify that ReconcileStretchAndScroll reduces the overscrolled delta.
TEST_F(OverscrollBounceControllerTest, ReconcileStretchAndScroll) {
  // Test overscroll in both directions.
  gfx::Vector2dF delta(0, -50);
  SendGestureScrollBegin(WebGestureEvent::InertialPhaseState::kNonMomentum);
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::ScrollOffset(5, 8),
                                            gfx::ScrollOffset(100, 100));
  SendGestureScrollUpdate(WebGestureEvent::InertialPhaseState::kNonMomentum,
                          delta, gfx::Vector2dF(-100, -100));
  EXPECT_EQ(gfx::Vector2dF(-19, -19), helper_.StretchAmount());
  controller_.ReconcileStretchAndScroll();
  EXPECT_EQ(gfx::Vector2dF(-18, -18), helper_.StretchAmount());
  // Adjustment of gfx::ScrollOffset(-5, -8) should bring back the
  // scroll_offset_ to 0.
  EXPECT_EQ(helper_.ScrollOffset(), gfx::ScrollOffset(0, 0));
}

// Tests if the overscrolled delta maps correctly to the actual amount that the
// scroller gets stretched.
TEST_F(OverscrollBounceControllerTest, VerifyOverscrollBounceDistance) {
  gfx::Vector2dF overscroll_bounce_distance(
      controller_.OverscrollBounceDistance(gfx::Vector2dF(0, -100),
                                           helper_.ScrollBounds()));
  EXPECT_EQ(overscroll_bounce_distance.y(), -19);

  overscroll_bounce_distance = controller_.OverscrollBounceDistance(
      gfx::Vector2dF(-100, 0), helper_.ScrollBounds());
  EXPECT_EQ(overscroll_bounce_distance.x(), -19);
}

}  // namespace test
}  // namespace blink
