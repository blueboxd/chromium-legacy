// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include "base/test/test_mock_time_task_runner.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TextPaintTimingDetectorTest
    : public RenderingTest,
      private ScopedFirstContentfulPaintPlusPlusForTest {
 public:
  TextPaintTimingDetectorTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedFirstContentfulPaintPlusPlusForTest(true) {}
  void SetUp() override {
    RenderingTest::SetUp();
    RenderingTest::EnableCompositing();
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    // Advance clock so it isn't 0 as rendering code asserts in that case.
    AdvanceClock(base::TimeDelta::FromMicroseconds(1));
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }

  IntRect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  LocalFrameView& GetChildFrameView() { return *ChildFrame().View(); }

  unsigned CountVisibleTexts() {
    if (!GetPaintTimingDetector().GetTextPaintTimingDetector())
      return 0u;

    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->records_manager_.visible_node_map_.size();
  }

  unsigned CountRankingSetSize() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->records_manager_.size_ordered_set_.size();
  }

  void InvokeCallback() {
    TextPaintTimingDetector* detector =
        GetPaintTimingDetector().GetTextPaintTimingDetector();
    detector->ReportSwapTime(WebWidgetClient::SwapResult::kDidSwap,
                             test_task_runner_->NowTicks());
  }

  base::TimeTicks LargestPaintStoredResult() {
    return GetPaintTimingDetector().largest_text_paint_time_;
  }

  // This only triggers ReportSwapTime in main frame.
  void UpdateAllLifecyclePhasesAndSimulateSwapTime() {
    UpdateAllLifecyclePhasesForTest();
    TextPaintTimingDetector* detector =
        GetPaintTimingDetector().GetTextPaintTimingDetector();
    if (detector &&
        !detector->records_manager_.texts_queued_for_paint_time_.empty()) {
      detector->ReportSwapTime(WebWidgetClient::SwapResult::kDidSwap,
                               test_task_runner_->NowTicks());
    }
  }

  size_t CountPendingSwapTime(LocalFrameView& frame_view) {
    TextPaintTimingDetector* detector =
        frame_view.GetPaintTimingDetector().GetTextPaintTimingDetector();
    return detector->records_manager_.texts_queued_for_paint_time_.size();
  }

  void ChildFrameSwapTimeCallBack() {
    GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->ReportSwapTime(WebWidgetClient::SwapResult::kDidSwap,
                         test_task_runner_->NowTicks());
  }

  void UpdateCandidate() {
    GetPaintTimingDetector().GetTextPaintTimingDetector()->UpdateCandidate();
  }

  Element* AppendFontBlockToBody(String content) {
    Element* font = GetDocument().CreateRawElement(html_names::kFontTag);
    font->setAttribute(html_names::kSizeAttr, AtomicString("5"));
    Text* text = GetDocument().createTextNode(content);
    font->AppendChild(text);
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->AppendChild(font);
    GetDocument().body()->AppendChild(div);
    return font;
  }

  Element* AppendDivElementToBody(String content, String style = "") {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->setAttribute(html_names::kStyleAttr, AtomicString(style));
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

  TextRecord* TextRecordOfLargestTextPaint() {
    return GetFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->FindLargestPaintCandidate();
  }

  TextRecord* ChildFrameTextRecordOfLargestTextPaint() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->FindLargestPaintCandidate();
  }

  void SetFontSize(Element* font_element, uint16_t font_size) {
    DCHECK_EQ(font_element->nodeName(), "FONT");
    font_element->setAttribute(html_names::kSizeAttr,
                               AtomicString(WTF::String::Number(font_size)));
  }

  void SetElementStyle(Element* element, String style) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(style));
  }

  void RemoveElement(Element* element) {
    element->GetLayoutObject()->Parent()->GetNode()->removeChild(element);
  }

  base::TimeTicks NowTicks() const { return test_task_runner_->NowTicks(); }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(only_text));
}

TEST_F(TextPaintTimingDetectorTest, AggregationBySelfPaintingInlineElement) {
  SetBodyInnerHTML(R"HTML(
    <div style="background: yellow">
      tiny
      <span id="target"
        style="position: relative; background: blue; top: 100px; left: 100px">
        this is the largest text in the world.</span>
    </div>
  )HTML");
  Element* span = GetDocument().getElementById("target");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(span));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OpacityZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      opacity: 0;
    }
    </style>
  )HTML");
  AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, NodeRemovedBeforeAssigningSwapTime) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("remove"));
  InvokeCallback();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("medium text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  Element* large_text = AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  AppendDivElementToBody("small");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(large_text));
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  base::TimeTicks time1 = NowTicks();
  SetBodyInnerHTML(R"HTML(
    <div>small text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  UpdateCandidate();
  base::TimeTicks time2 = NowTicks();
  base::TimeTicks first_largest = LargestPaintStoredResult();
  EXPECT_GE(first_largest, time1);
  EXPECT_GE(time2, first_largest);

  AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  UpdateCandidate();
  base::TimeTicks time3 = NowTicks();
  base::TimeTicks second_largest = LargestPaintStoredResult();
  EXPECT_GE(second_largest, time2);
  EXPECT_GE(time3, second_largest);
}

// There is a risk that a text that is just recorded is selected to be the
// metric candidate. The algorithm should skip the text record if its paint time
// hasn't been recorded yet.
TEST_F(TextPaintTimingDetectorTest, PendingTextIsLargest) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  // We do not call swap-time callback here in order to not set the paint time.
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

// The same node may be visited by recordText for twice before the paint time
// is set. In some previous design, this caused the node to be recorded twice.
TEST_F(TextPaintTimingDetectorTest, VisitSameNodeTwiceBeforePaintTimeIsSet) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  // Change a property of the text to trigger repaint.
  text->setAttribute(html_names::kStyleAttr, AtomicString("color:red;"));
  GetFrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  InvokeCallback();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(text));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  base::TimeTicks start_time = NowTicks();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  text->setAttribute(html_names::kStyleAttr,
                     AtomicString("position:fixed;left:30px"));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time, start_time + base::TimeDelta::FromSecondsD(1));
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_IgnoreTextOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div.out {
        position: fixed;
        top: -100px;
      }
    </style>
    <div class='out'>text outside of viewport</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_IgnoreRemovedText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  Element* small_text = AppendDivElementToBody("small text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(large_text));

  RemoveElement(large_text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(small_text));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportLastNullCandidate) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text to remove");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  UpdateCandidate();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(text));
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());

  RemoveElement(text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  UpdateCandidate();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
  EXPECT_EQ(LargestPaintStoredResult(), base::TimeTicks());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(short_text));
}

// Depite that the l
TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_CompareSizesAtFirstPaint) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* shortening_long_text = AppendDivElementToBody("123456789");
  AppendDivElementToBody("12345678");  // 1 letter shorter than the above.
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  // The visual size becomes smaller when less portion intersecting with
  // viewport.
  SetElementStyle(shortening_long_text, "position:fixed;left:-10px");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(shortening_long_text));
}

TEST_F(TextPaintTimingDetectorTest, TreatEllipsisAsText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div style="font:10px Ahem;white-space:nowrap;width:50px;overflow:hidden;text-overflow:ellipsis;">
    00000000000000000000000000000000000000000000000000000000000000000000000000
    00000000000000000000000000000000000000000000000000000000000000000000000000
    </div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureFileUploadController) {
  SetBodyInnerHTML("<input type='file'>");
  Element* element = GetDocument().QuerySelector("input");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(element));
}

TEST_F(TextPaintTimingDetectorTest, NotCapturingListMarkers) {
  SetBodyInnerHTML(R"HTML(
    <ul>
      <li></li>
    </ul>
    <ol>
      <li></li>
    </ol>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg height="40" width="300">
      <text x="0" y="15">A SVG text.</text>
    </svg>
  )HTML");

  SVGTextContentElement* elem =
      ToSVGTextContentElement(GetDocument().QuerySelector("text"));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(elem));
}

TEST_F(TextPaintTimingDetectorTest, StopRecordingOverNodeLimit) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  for (int i = 1; i <= 4999; i++)
    AppendDivElementToBody(WTF::String::Number(i), "position:fixed;left:0px");

  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  AppendDivElementToBody(WTF::String::Number(5000));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  // Reached limit, so stopped recording and now should have 0 texts.
  EXPECT_EQ(CountVisibleTexts(), 0u);

  AppendDivElementToBody(WTF::String::Number(5001));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

// This is for comparison with the ClippedByViewport test.
TEST_F(TextPaintTimingDetectorTest, NormalTextUnclipped) {
  SetBodyInnerHTML(R"HTML(
    <div id='d'>text</div>
  )HTML");
  EXPECT_EQ(CountPendingSwapTime(GetFrameView()), 1u);
  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d { margin-top: 1234567px }
    </style>
    <div id='d'>text</div>
  )HTML");
  // Make sure the margin-top is larger than the viewport height.
  DCHECK_LT(GetViewportRect(GetFrameView()).Height(), 1234567);
  EXPECT_EQ(CountPendingSwapTime(GetFrameView()), 0u);
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByParentVisibleRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #outer1 {
        overflow: hidden;
        height: 1px;
        width: 1px;
      }
      #outer2 {
        overflow: hidden;
        height: 2px;
        width: 2px;
      }
    </style>
    <div id='outer1'></div>
    <div id='outer2'></div>
  )HTML");
  Element* div1 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text1 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div1->AppendChild(text1);
  GetDocument().body()->getElementById("outer1")->AppendChild(div1);

  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(div1));
  EXPECT_EQ(TextRecordOfLargestTextPaint()->first_size, 1u);

  Element* div2 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text2 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div2->AppendChild(text2);
  GetDocument().body()->getElementById("outer2")->AppendChild(div2);

  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(div2));
  // This size is larger than the size of the first object . But the exact size
  // depends on different platforms. We only need to ensure this size is larger
  // than the first size.
  EXPECT_GT(TextRecordOfLargestTextPaint()->first_size, 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameHTML("A");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountPendingSwapTime(GetChildFrameView()), 1u);
  ChildFrameSwapTimeCallBack();
  TextRecord* text = ChildFrameTextRecordOfLargestTextPaint();
  EXPECT_TRUE(text);
}

TEST_F(TextPaintTimingDetectorTest, Iframe_ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      #d { margin-top: 200px }
    </style>
    <div id='d'>text</div>
  )HTML");
  DCHECK_EQ(GetViewportRect(GetChildFrameView()).Height(), 100);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountPendingSwapTime(GetChildFrameView()), 0u);
}

TEST_F(TextPaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetBodyInnerHTML(R"HTML(
    <div>text</div>
    <div>text</div>
    <div>text</div>
    <div>text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(CountRankingSetSize(), 4u);
}

}  // namespace blink
