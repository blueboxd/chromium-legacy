// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ScrollTimelineTest : public RenderingTest {
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

 public:
  void SimulateFrame() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() +
                    base::TimeDelta::FromMilliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }
};

class TestScrollTimeline : public ScrollTimeline {
 public:
  TestScrollTimeline(
      Document* document,
      Element* scroll_source,
      CSSPrimitiveValue* start_scroll_offset =
          CSSNumericLiteralValue::Create(10.0,
                                         CSSPrimitiveValue::UnitType::kPixels),
      CSSPrimitiveValue* end_scroll_offset =
          CSSNumericLiteralValue::Create(90.0,
                                         CSSPrimitiveValue::UnitType::kPixels))
      : ScrollTimeline(document,
                       scroll_source,
                       ScrollTimeline::Vertical,
                       start_scroll_offset,
                       end_scroll_offset,
                       100.0),
        next_service_scheduled_(false) {}

  void ScheduleServiceOnNextFrame() override {
    ScrollTimeline::ScheduleServiceOnNextFrame();
    next_service_scheduled_ = true;
  }
  void Trace(Visitor* visitor) override { ScrollTimeline::Trace(visitor); }
  bool NextServiceScheduled() const { return next_service_scheduled_; }
  void ResetNextServiceScheduled() { next_service_scheduled_ = false; }

 private:
  bool next_service_scheduled_;
};

TEST_F(ScrollTimelineTest, CurrentTimeIsNullIfScrollSourceIsNotScrollable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { width: 100px; height: 100px; }</style>
    <div id='scroller'></div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = false;
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_TRUE(current_time_is_null);
  EXPECT_FALSE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfScrollOffsetIsBeyondStartAndEndScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset("10px");
  options->setEndScrollOffset("90px");
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 5),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current time.
  SimulateFrame();
  double current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(current_time, 0);
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(current_time, 0);
  EXPECT_EQ("active", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(current_time, 50);
  EXPECT_EQ("active", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 90),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(current_time, time_range.GetAsDouble());
  EXPECT_EQ("after", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(current_time, time_range.GetAsDouble());
  EXPECT_EQ("after", scroll_timeline->phase());
  EXPECT_TRUE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfEndScrollOffsetIsLessThanStartScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset("80px");
  options->setEndScrollOffset("40px");
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current time.
  SimulateFrame();
  double current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(0, current_time);
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 60),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(0, current_time);
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  current_time = scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);
  EXPECT_EQ(time_range.GetAsDouble(), current_time);
  EXPECT_EQ("after", scroll_timeline->phase());
  EXPECT_TRUE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest, PhasesAreCorrectWhenUsingOffsets) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset("10px");
  options->setEndScrollOffset("90px");
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(scroll_timeline->phase(), "before");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current phase and time.
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "active");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "active");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 90),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "after");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "after");
}

TEST_F(ScrollTimelineTest,
       UsingDocumentScrollingElementShouldCorrectlyResolveToDocument) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #content { width: 10000px; height: 10000px; }
    </style>
    <div id='content'></div>
  )HTML");

  EXPECT_EQ(GetDocument().documentElement(), GetDocument().scrollingElement());
  // Create the ScrollTimeline with Document.scrollingElement() as source. The
  // resolved scroll source should be the Document.
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());
}

TEST_F(ScrollTimelineTest,
       ChangingDocumentScrollingElementShouldNotImpactScrollTimeline) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #body { overflow: scroll; width: 100px; height: 100px; }
      #content { width: 10000px; height: 10000px; }
    </style>
    <div id='content'></div>
  )HTML");

  // In QuirksMode, the body is the scrolling element
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  EXPECT_EQ(GetDocument().body(), GetDocument().scrollingElement());

  // Create the ScrollTimeline with Document.scrollingElement() as source. The
  // resolved scroll source should be the Document.
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());

  // Now change the Document.scrollingElement(). In NoQuirksMode, the
  // documentElement is the scrolling element and not the body.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  EXPECT_NE(GetDocument().documentElement(), GetDocument().body());
  EXPECT_EQ(GetDocument().documentElement(), GetDocument().scrollingElement());

  // Changing the scrollingElement should not impact the previously resolved
  // scroll source. Note that at this point the scroll timeline's scroll source
  // is still body element which is no longer the scrolling element. So if we
  // were to re-resolve the scroll source, it would not map to Document.
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());
}

TEST_F(ScrollTimelineTest, AttachOrDetachAnimationWithNullScrollSource) {
  // Directly call the constructor to make it easier to pass a null
  // scrollSource. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* scroll_source = nullptr;
  CSSPrimitiveValue* start_scroll_offset = nullptr;
  CSSPrimitiveValue* end_scroll_offset = nullptr;
  Persistent<ScrollTimeline> scroll_timeline =
      MakeGarbageCollected<ScrollTimeline>(
          &GetDocument(), scroll_source, ScrollTimeline::Block,
          start_scroll_offset, end_scroll_offset, 100);

  // Sanity checks.
  ASSERT_EQ(scroll_timeline->scrollSource(), nullptr);
  ASSERT_EQ(scroll_timeline->ResolvedScrollSource(), nullptr);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  Animation* animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  EXPECT_EQ(1u, scroll_timeline->GetAnimations().size());
  EXPECT_TRUE(scroll_timeline->GetAnimations().Contains(animation));

  animation = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, scroll_timeline->GetAnimations().size());
}

TEST_F(ScrollTimelineTest, ScheduleFrameOnlyWhenScrollOffsetChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);

  Element* scroller_element = GetElementById("scroller");
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();

  // Validate that no frame is scheduled when there is no scroll change.
  scroll_timeline->ResetNextServiceScheduled();
  scroll_timeline->ScheduleNextService();
  EXPECT_FALSE(scroll_timeline->NextServiceScheduled());

  // Validate that frame is scheduled when scroll changes.
  scroll_timeline->ResetNextServiceScheduled();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 30),
                                   mojom::blink::ScrollType::kProgrammatic);
  scroll_timeline->ScheduleNextService();
  EXPECT_TRUE(scroll_timeline->NextServiceScheduled());
}

// This test verifies scenario when scroll timeline is updated as a result of
// layout run. In this case the expectation is that at the end of paint
// lifecycle phase scroll timeline schedules a new frame that runs animations
// update.
TEST_F(ScrollTimelineTest, ScheduleFrameWhenScrollerLayoutChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  Element* scroller_element = GetElementById("scroller");
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(), scroller_element,
                                               nullptr, nullptr);
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();
  // Validate that frame is scheduled when scroller layout changes.
  Element* spacer_element = GetElementById("spacer");
  spacer_element->setAttribute(html_names::kStyleAttr, "height:1000px;");
  scroll_timeline->ResetNextServiceScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(scroll_timeline->NextServiceScheduled());
}

// Verify that scroll timeline current time is updated once upon construction
// and at the top of every animation frame.
TEST_F(ScrollTimelineTest, CurrentTimeUpdateAfterNewAnimationFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));

  scrollable_area->SetScrollOffset(ScrollOffset(0, 5),
                                   mojom::blink::ScrollType::kProgrammatic);

  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = false;
  double time_before = scroll_timeline->currentTime(current_time_is_null);
  ASSERT_FALSE(current_time_is_null);

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Verify that the current time didn't change before there is a new animation
  // frame.
  EXPECT_EQ(time_before, scroll_timeline->currentTime(current_time_is_null));
  ASSERT_FALSE(current_time_is_null);

  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();

  // Verify that current time did change in the new animation frame.
  EXPECT_NE(time_before, scroll_timeline->currentTime(current_time_is_null));
  ASSERT_FALSE(current_time_is_null);
}

}  //  namespace blink
