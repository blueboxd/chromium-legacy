// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_touch_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

TEST(SyntheticWebInputEventBuilders, BuildWebTouchEvent) {
  SyntheticWebTouchEvent event;

  event.PressPoint(1, 2);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(0, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::kStatePressed, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(1, 2), event.touches[0].PositionInWidget());
  event.ResetPoints();

  event.PressPoint(3, 4);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::kStatePressed, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(3, 4), event.touches[1].PositionInWidget());
  event.ResetPoints();

  event.MovePoint(1, 5, 6);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::kStateMoved, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(5, 6), event.touches[1].PositionInWidget());
  event.ResetPoints();

  event.ReleasePoint(0);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(0, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::kStateReleased, event.touches[0].state);
  event.ResetPoints();

  event.MovePoint(1, 7, 8);
  EXPECT_EQ(1U, event.touches_length);
  EXPECT_EQ(1, event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::kStateMoved, event.touches[1].state);
  EXPECT_EQ(gfx::PointF(7, 8), event.touches[1].PositionInWidget());
  EXPECT_EQ(WebTouchPoint::kStateUndefined, event.touches[0].state);
  event.ResetPoints();

  event.PressPoint(9, 10);
  EXPECT_EQ(2U, event.touches_length);
  EXPECT_EQ(2, event.touches[0].id);
  EXPECT_EQ(WebTouchPoint::kStatePressed, event.touches[0].state);
  EXPECT_EQ(gfx::PointF(9, 10), event.touches[0].PositionInWidget());
}

}  // namespace content
