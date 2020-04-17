// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HitTestingTest : public RenderingTest {
 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }

  PositionWithAffinity HitTest(const PhysicalOffset offset) {
    const HitTestRequest hit_request(HitTestRequest::kActive);
    const HitTestLocation hit_location(offset);
    HitTestResult hit_result(hit_request, hit_location);
    if (!GetLayoutView().HitTest(hit_location, hit_result))
      return PositionWithAffinity();
    // Simulate |PositionWithAffinityOfHitTestResult()| in
    // "selection_controller.cc"
    LayoutObject* const layout_object =
        hit_result.InnerPossiblyPseudoNode()->GetLayoutObject();
    if (!layout_object)
      return PositionWithAffinity();
    return layout_object->PositionForPoint(hit_result.LocalPoint());
  }
};

// http://crbug.com/1043471
TEST_F(HitTestingTest, PseudoElementAfter) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0px; font: 10px/10px Ahem; }"
      "#cd::after { content: 'XYZ'; margin-left: 100px; }");
  SetBodyInnerHTML("<div id=ab>ab<span id=cd>cd</span></div>");
  const auto& text_ab = *To<Text>(GetElementById("ab")->firstChild());
  const auto& text_cd = *To<Text>(GetElementById("cd")->lastChild());

  EXPECT_EQ(PositionWithAffinity(Position(text_ab, 0)),
            HitTest(PhysicalOffset(5, 5)));
  // Because of hit testing at "b", position should be |kDownstream|.
  EXPECT_EQ(PositionWithAffinity(Position(text_ab, 1),
                                 LayoutNGEnabled() ? TextAffinity::kDownstream
                                                   : TextAffinity::kUpstream),
            HitTest(PhysicalOffset(15, 5)));
  EXPECT_EQ(PositionWithAffinity(Position(text_cd, 0)),
            HitTest(PhysicalOffset(25, 5)));
  // Because of hit testing at "d", position should be |kDownstream|.
  EXPECT_EQ(PositionWithAffinity(Position(text_cd, 1),
                                 LayoutNGEnabled() ? TextAffinity::kDownstream
                                                   : TextAffinity::kUpstream),
            HitTest(PhysicalOffset(35, 5)));
  // Because of hit testing at right of <span cd>, result position should be
  // |kUpstream|.
  EXPECT_EQ(PositionWithAffinity(Position(text_cd, 2),
                                 LayoutNGEnabled() ? TextAffinity::kUpstream
                                                   : TextAffinity::kDownstream),
            HitTest(PhysicalOffset(45, 5)));
  EXPECT_EQ(PositionWithAffinity(Position(text_cd, 2),
                                 LayoutNGEnabled() ? TextAffinity::kUpstream
                                                   : TextAffinity::kDownstream),
            HitTest(PhysicalOffset(55, 5)));
  EXPECT_EQ(PositionWithAffinity(Position(text_cd, 2),
                                 LayoutNGEnabled() ? TextAffinity::kUpstream
                                                   : TextAffinity::kDownstream),
            HitTest(PhysicalOffset(65, 5)));
}

TEST_F(HitTestingTest, OcclusionHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    </style>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

TEST_F(HitTestingTest, OcclusionHitTestWithClipPath) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    #occluder {
      clip-path: url(#clip);
    }
    </style>

    <svg viewBox="0 0 100 100" width=0>
      <clipPath id="clip">
        <circle cx="50" cy="50" r="45" stroke="none" />
      </clipPath>
    </svg>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");

  // target and occluder don't overlap, no occlusion.
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and occluder layout rects overlap, but the overlapping area of the
  // occluder is clipped out, so no occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-4px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and clipped area of occluder overlap, so there is occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-6px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

}  // namespace blink
