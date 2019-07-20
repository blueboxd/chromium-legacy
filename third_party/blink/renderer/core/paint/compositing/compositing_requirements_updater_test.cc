// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_requirements_updater.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

namespace blink {

class CompositingRequirementsUpdaterTest : public RenderingTest {
 public:
  CompositingRequirementsUpdaterTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() final;
};

void CompositingRequirementsUpdaterTest::SetUp() {
  EnableCompositing();
  RenderingTest::SetUp();
}

TEST_F(CompositingRequirementsUpdaterTest,
       NoOverlapReasonForNonSelfPaintingLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
       overflow: auto;
       width: 100px;
       height: 100px;
       margin-top: -50px;
     }
    </style>
    <div style="position: relative; width: 500px; height: 300px;
        will-change: transform"></div>
    <div id=target></div>
  )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_FALSE(target->GetCompositingReasons());

  // Now make |target| self-painting.
  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(CompositingReason::kOverlap, target->GetCompositingReasons());
}

TEST_F(CompositingRequirementsUpdaterTest,
       NoDescendantReasonForNonSelfPaintingLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        overflow: auto;
        width: 100px;
        height: 100px;
      }
    </style>
    <div id=target>
      <div style="backface-visibility: hidden"></div>
    </div>
  )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_FALSE(target->GetCompositingReasons());

  // Now make |target| self-painting.
  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReason::kClipsCompositingDescendants,
            target->GetCompositingReasons());
}

// This test sets up a situation where a squashed PaintLayer loses its
// backing, but does not change visual rect. Therefore the compositing system
// must invalidate it because of change of backing.
TEST_F(CompositingRequirementsUpdaterTest,
       NeedsLayerAssignmentAfterSquashingRemoval) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
      #target {
        width: 100px; height: 100px; backface-visibility: hidden
      }
      div {
        width: 100px; height: 100px;
        position: absolute;
        background: lightblue;
        top: 0px;
      }
    </style>
    <div id=target></div>
    <div id=squashed></div>
  )HTML");

  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  GetDocument().View()->SetTracksPaintInvalidations(true);

  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "display: none");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(kNotComposited, squashed->GetCompositingState());
  auto* tracking = GetDocument()
                       .View()
                       ->GetLayoutView()
                       ->Layer()
                       ->GraphicsLayerBacking()
                       ->GetRasterInvalidationTracking();
  EXPECT_TRUE(tracking->HasInvalidations());

  EXPECT_EQ(IntRect(0, 0, 100, 100), tracking->Invalidations()[0].rect);
}

}  // namespace blink
