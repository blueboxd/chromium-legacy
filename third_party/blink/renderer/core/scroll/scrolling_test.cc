// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_animator.h"

#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FractionalScrollSimTest : public SimTest {
 public:
  FractionalScrollSimTest() : fractional_scroll_offsets_for_test_(true) {}

 private:
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets_for_test_;
};

TEST_F(FractionalScrollSimTest, GetBoundingClientRectAtFractional) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 2000px;
        width: 2000px;
      }
      div {
        position: absolute;
        left: 800px;
        top: 600px;
        width: 100px;
        height: 100px;
      }
    </style>
    <body>
      <div id="target"></div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  // Scroll on the layout viewport.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      FloatSize(700.5f, 500.6f), kProgrammaticScroll, kScrollBehaviorInstant);

  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  DOMRect* rect = target->getBoundingClientRect();
  const float kOneLayoutUnit = 1.f / kFixedPointDenominator;
  EXPECT_NEAR(LayoutUnit(800.f - 700.5f), rect->left(), kOneLayoutUnit);
  EXPECT_NEAR(LayoutUnit(600.f - 500.6f), rect->top(), kOneLayoutUnit);
}

class ScrollAnimatorSimTest : public SimTest {};

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at the
// layout viewport.
TEST_F(ScrollAnimatorSimTest, TestRootFrameLayoutViewportUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  // Scroll on the layout viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(100, 300),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(0.3);
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at the
// visual viewport.
TEST_F(ScrollAnimatorSimTest, TestRootFrameVisualViewporUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);
  WebView().SetPageScaleFactor(2);

  // Scroll on the visual viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(100, 300),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(0.3);
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at both
// the layout and visual viewport.
TEST_F(ScrollAnimatorSimTest, TestRootFrameBothViewporsUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);
  WebView().SetPageScaleFactor(2);

  // Scroll on both the layout and visual viewports.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(0, 1000),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(0.3);
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for div user scroll.
TEST_F(ScrollAnimatorSimTest, TestDivUserScrollCallBack) {
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        width: 100px;
        height: 100px;
        overflow: auto;
      }
      #overflow {
        height: 500px;
        width: 500px;
      }
    </style>
    <div id="scroller">
      <div id="overflow"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  Element* scroller = GetDocument().getElementById("scroller");

  bool finished = false;
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBox(scroller->GetLayoutObject())->GetScrollableArea();
  scrollable_area->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(0, 100),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame(0.3);
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed in
// ScrollAnimatorBase::UserScroll when animation is disabled.
TEST_F(ScrollAnimatorSimTest, TestUserScrollCallBackAnimatorDisabled) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(0, 300),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));
  Compositor().BeginFrame();
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation is
// canceled because performing a programmatic scroll in the middle of a user
// scroll will cancel the animation.
TEST_F(ScrollAnimatorSimTest, TestRootFrameUserScrollCallBackCancelAnimation) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameWidget()->Resize(WebSize(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  // Scroll on the layout viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ScrollGranularity::kScrollByLine, FloatSize(100, 300),
      ScrollableArea::ScrollCallback(
          base::BindLambdaForTesting([&]() { finished = true; })));

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // Programmatic scroll will cancel the current user scroll animation and the
  // callback will be executed.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), kProgrammaticScroll, kScrollBehaviorSmooth,
      ScrollableArea::ScrollCallback());
  Compositor().BeginFrame();
  ASSERT_TRUE(finished);
}

}  // namespace blink
