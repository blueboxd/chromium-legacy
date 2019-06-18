// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/jank_tracker.h"

#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class JankTrackerTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  JankTracker& GetJankTracker() { return GetFrameView().GetJankTracker(); }

  void SimulateInput() {
    GetJankTracker().NotifyInput(WebMouseEvent(
        WebInputEvent::kMouseDown, WebFloatPoint(), WebFloatPoint(),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, CurrentTimeTicks()));
  }

  void UpdateAllLifecyclePhases() {
    GetFrameView().UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }
};

TEST_F(JankTrackerTest, SimpleBlockMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");

  EXPECT_EQ(0.0, GetJankTracker().Score());
  EXPECT_EQ(0.0, GetJankTracker().OverallMaxDistance());

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 60px"));
  UpdateAllLifecyclePhases();
  // 300 * (100 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
  // ScoreWithMoveDistance should be scaled by the amount that the content moved
  // (60px) relative to the max viewport dimension (width=800px).
  EXPECT_FLOAT_EQ(0.1 * (60.0 / 800.0),
                  GetJankTracker().ScoreWithMoveDistance());
  EXPECT_FLOAT_EQ(60.0, GetJankTracker().OverallMaxDistance());
}

TEST_F(JankTrackerTest, GranularitySnapping) {
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 304px; height: 104px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 58px"));
  UpdateAllLifecyclePhases();
  // Rect locations and sizes should snap to multiples of 600 / 60 = 10.
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #c { transform: translateX(-300px) translateY(-40px); }
      #j { position: relative; width: 600px; height: 140px; }
    </style>
    <div id='c'>
      <div id='j'></div>
    </div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 60px"));
  UpdateAllLifecyclePhases();
  // (600 - 300) * (140 - 40 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, RtlDistance) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 100px; height: 100px; direction: rtl; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(
      html_names::kStyleAttr, AtomicString("width: 70px; left: 10px"));
  UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(20.0, GetJankTracker().OverallMaxDistance());
}

TEST_F(JankTrackerTest, SmallMovementIgnored) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 2px"));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, SmallMovementIgnoredWithZoom) {
  GetDocument().GetFrame()->SetPageZoomFactor(2);
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 2px"));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, IgnoreAfterInput) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 60px"));
  SimulateInput();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
  EXPECT_TRUE(GetJankTracker().ObservedInputOrScroll());
}

TEST_F(JankTrackerTest, CompositedElementMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #jank {
      position: relative;
      width: 500px;
      height: 200px;
      background: yellow;
    }
    #container {
      position: absolute;
      width: 400px;
      height: 400px;
      left: 400px;
      top: 100px;
      background: #ccc;
    }
    .tr { will-change: transform; }
    </style>
    <div id='container' class='tr'>
      <div id='space'></div>
      <div id='jank' class='tr'></div>
    </div>
  )HTML");

  GetDocument().getElementById("space")->setAttribute(
      html_names::kStyleAttr, AtomicString("height: 100px"));
  UpdateAllLifecyclePhases();

  // #jank is 400x200 after viewport intersection with correct application of
  // composited #container offset, and 100px lower after janking, so jank score
  // is (400 * 300) / (viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.25, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, CompositedJankBeforeFirstPaint) {
  // Tests that we don't crash if a new layer janks during a second compositing
  // update before prepaint sets up property tree state.  See crbug.com/881735
  // (which invokes UpdateLifecycleToCompositingCleanPlusScrolling through
  // accessibilityController.accessibleElementById).

  SetBodyInnerHTML(R"HTML(
    <style>
      .hide { display: none; }
      .tr { will-change: transform; }
      body { margin: 0; }
      div { height: 100px; }
    </style>
    <div id="container">
      <div id="A">A</div>
      <div id="B" class="tr hide">B</div>
    </div>
  )HTML");

  GetDocument().getElementById("B")->setAttribute(html_names::kClassAttr,
                                                  AtomicString("tr"));
  GetFrameView().UpdateLifecycleToCompositingCleanPlusScrolling();
  GetDocument().getElementById("A")->setAttribute(html_names::kClassAttr,
                                                  AtomicString("hide"));
  UpdateAllLifecyclePhases();
}

TEST_F(JankTrackerTest, IgnoreFixedAndSticky) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body { height: 1000px; }
    #f1, #f2 {
      position: fixed;
      width: 300px;
      height: 100px;
      left: 100px;
    }
    #f1 { top: 0; }
    #f2 { top: 150px; will-change: transform; }
    #s1 {
      position: sticky;
      width: 200px;
      height: 100px;
      left: 450px;
      top: 0;
    }
    </style>
    <div id='f1'>fixed</div>
    <div id='f2'>fixed composited</div>
    <div id='s1'>sticky</div>
    normal
  )HTML");

  GetDocument().scrollingElement()->setScrollTop(50);
  UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, IgnoreSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <circle cx="50" cy="50" r="40"
              stroke="black" stroke-width="3" fill="red" />
    </svg>
  )HTML");
  GetDocument().QuerySelector("circle")->setAttribute(svg_names::kCxAttr,
                                                      AtomicString("100"));
  UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, JankWhileScrolled) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { height: 1000px; margin: 0; }
      #j { position: relative; width: 300px; height: 200px; }
    </style>
    <div id='j'></div>
  )HTML");

  GetDocument().scrollingElement()->setScrollTop(100);
  EXPECT_EQ(0.0, GetJankTracker().Score());
  EXPECT_EQ(0.0, GetJankTracker().OverallMaxDistance());

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 60px"));
  UpdateAllLifecyclePhases();
  // 300 * (height 200 - scrollY 100 + movement 60) / (800 * 600 viewport)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, FullyClippedVisualRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #clip { width: 0px; height: 600px; overflow: hidden; }
      #j { position: relative; width: 300px; height: 200px; }
    </style>
    <div id='clip'><div id='j'></div></div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 200px"));
  UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, PartiallyClippedVisualRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #clip { width: 150px; height: 600px; overflow: hidden; }
      #j { position: relative; width: 300px; height: 200px; }
    </style>
    <div id='clip'><div id='j'></div></div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 200px"));
  UpdateAllLifecyclePhases();
  // (clipped width 150px) * (height 200 + movement 200) / (800 * 600 viewport)
  EXPECT_FLOAT_EQ(0.125, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, MultiClipVisualRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #outer { width: 200px; height: 600px; overflow: hidden; }
      #inner { width: 300px; height: 150px; overflow: hidden; }
      #j { position: relative; width: 300px; height: 600px; }
    </style>
    <div id='outer'><div id='inner'><div id='j'></div></div></div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: -200px"));
  UpdateAllLifecyclePhases();
  // Note that, while the element moves up 200px, its visibility is
  // clipped at 0px,150px height, so the additional 200px of vertical
  // move distance is not included in the score.
  // (clip width 200) * (clip height 150) / (800 * 600 viewport)
  EXPECT_FLOAT_EQ(0.0625, GetJankTracker().Score());
  EXPECT_FLOAT_EQ(200.0, GetJankTracker().OverallMaxDistance());
}

TEST_F(JankTrackerTest, ShiftOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #j { position: relative; width: 600px; height: 200px; top: 600px; }
    </style>
    <div id='j'></div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 800px"));
  UpdateAllLifecyclePhases();
  // Since the element moves entirely outside of the viewport, it shouldn't
  // generate a score.
  EXPECT_FLOAT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, ShiftInToViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #j { position: relative; width: 600px; height: 200px; top: 600px; }
    </style>
    <div id='j'></div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 400px"));
  UpdateAllLifecyclePhases();
  // The element moves from outside the viewport to within the viewport, which
  // should generate jank.
  // (width 600) * (height 0 + move 200) / (800 * 600 viewport)
  EXPECT_FLOAT_EQ(0.25, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, ClipWithoutPaintLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 200px; height: 500px; }
      #space { height: 1000px; margin-bottom: -500px; }
      #j { width: 150px; height: 150px; background: yellow; }
    </style>
    <div id='scroller'>
      <div id='space'></div>
      <div id='j'></div>
    </div>
  )HTML");

  // Increase j's top margin by 100px. Since j is clipped by the scroller, this
  // should not generate jank. However, due to the issue in crbug/971639, this
  // case was erroneously reported as janking, before that bug was fixed. This
  // test ensures we do not regress this behavior.
  GetDocument().getElementById("j")->setAttribute(
      html_names::kStyleAttr, AtomicString("margin-top: 100px"));

  UpdateAllLifecyclePhases();
  // Make sure no jank score is reported, since the element that moved is fully
  // clipped by the scroller.
  EXPECT_FLOAT_EQ(0.0, GetJankTracker().Score());
}

class JankTrackerSimTest : public SimTest {};

TEST_F(JankTrackerSimTest, SubframeWeighting) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  // TODO(crbug.com/943668): Test OOPIF path.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_resource("https://example.com/sub.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style> #i { border: 0; position: absolute; left: 0; top: 0; } </style>
    <iframe id=i width=400 height=300 src='sub.html'></iframe>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_resource.Complete(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WebLocalFrameImpl& child_frame =
      To<WebLocalFrameImpl>(*MainFrame().FirstChild());

  Element* div = child_frame.GetFrame()->GetDocument()->getElementById("j");
  div->setAttribute(html_names::kStyleAttr, AtomicString("top: 60px"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // 300 * (100 + 60) / (default viewport size 800 * 600)
  JankTracker& jank_tracker = child_frame.GetFrameView()->GetJankTracker();
  EXPECT_FLOAT_EQ(0.4, jank_tracker.Score());
  EXPECT_FLOAT_EQ(0.1, jank_tracker.WeightedScore());

  // Move subframe halfway outside the viewport.
  GetDocument().getElementById("i")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("left: 600px"));

  div->removeAttribute(html_names::kStyleAttr);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FLOAT_EQ(0.8, jank_tracker.Score());
  EXPECT_FLOAT_EQ(0.15, jank_tracker.WeightedScore());
}

TEST_F(JankTrackerSimTest, ViewportSizeChange) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      body { margin: 0; }
      .square {
        display: inline-block;
        position: relative;
        width: 300px;
        height: 300px;
        background:yellow;
      }
    </style>
    <div class='square'></div>
    <div class='square'></div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Resize the viewport, making it 400px wide. This should cause the second div
  // to change position during block layout flow. Since it was the result of a
  // viewport size change, this position change should not affect the score.
  WebView().MainFrameWidget()->Resize(WebSize(400, 600));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  JankTracker& jank_tracker = MainFrame().GetFrameView()->GetJankTracker();
  EXPECT_FLOAT_EQ(0.0, jank_tracker.Score());
}

TEST_F(JankTrackerTest, StableCompositingChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #outer {
        margin-left: 50px;
        margin-top: 50px;
        width: 200px;
        height: 200px;
        background: #dde;
      }
      .tr {
        will-change: transform;
      }
      .pl {
        position: relative;
        z-index: 0;
        left: 0;
        top: 0;
      }
      #inner {
        display: inline-block;
        width: 100px;
        height: 100px;
        background: #666;
        margin-left: 50px;
        margin-top: 50px;
      }
    </style>
    <div id=outer><div id=inner></div></div>
  )HTML");

  Element* element = GetDocument().getElementById("outer");
  size_t state = 0;
  auto advance = [this, element, &state]() -> bool {
    //
    // Test each of the following transitions:
    // - add/remove a PaintLayer
    // - add/remove a cc::Layer when there is already a PaintLayer
    // - add/remove a cc::Layer and a PaintLayer together

    static const char* states[] = {"", "pl", "pl tr", "pl", "", "tr", ""};
    element->setAttribute(html_names::kClassAttr, AtomicString(states[state]));
    UpdateAllLifecyclePhases();
    return ++state < sizeof states / sizeof *states;
  };
  while (advance()) {
  }
  EXPECT_FLOAT_EQ(0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, CompositedOverflowExpansion) {
  SetBodyInnerHTML(R"HTML(
    <style>

    html { will-change: transform; }
    body { height: 2000px; margin: 0; }
    #drop {
      position: absolute;
      width: 1px;
      height: 1px;
      left: -10000px;
      top: -1000px;
    }
    .pl {
      position: relative;
      background: #ddd;
      z-index: 0;
      width: 290px;
      height: 170px;
      left: 25px;
      top: 25px;
    }
    #comp {
      position: relative;
      width: 240px;
      height: 120px;
      background: #efe;
      will-change: transform;
      z-index: 0;
      left: 25px;
      top: 25px;
    }
    .sh {
      top: 515px !important;
    }

    </style>
    <div class="pl">
      <div id="comp"></div>
    </div>
    <div id="drop" style="display: none"></div>
  )HTML");

  Element* drop = GetDocument().getElementById("drop");
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhases();

  drop->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(0, GetJankTracker().Score());

  Element* comp = GetDocument().getElementById("comp");
  comp->setAttribute(html_names::kClassAttr, AtomicString("sh"));
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhases();

  // old rect (240 * 120) / (800 * 600) = 0.06
  // new rect, 50% clipped by viewport (240 * 60) / (800 * 600) = 0.03
  // final score 0.06 + 0.03 = 0.09
  EXPECT_FLOAT_EQ(0.09, GetJankTracker().Score());
}

}  // namespace blink
