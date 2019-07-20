// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

class ObjectPaintInvalidatorTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

using PaintInvalidation = LocalFrameView::ObjectPaintInvalidation;
using ::testing::ElementsAre;

TEST_F(ObjectPaintInvalidatorTest,
       TraverseNonCompositingDescendantsInPaintOrder) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>div { width: 10px; height: 10px; background-color: green;
    }</style>
    <div id='container' style='position: fixed'>
      <div id='normal-child'></div>
      <div id='stacked-child' style='position: relative'></div>
      <div id='composited-stacking-context' style='will-change: transform'>
        <div id='normal-child-of-composited-stacking-context'></div>
        <div id='stacked-child-of-composited-stacking-context'
    style='position: relative'></div>
      </div>
      <div id='composited-non-stacking-context' style='backface-visibility:
    hidden'>
        <div id='normal-child-of-composited-non-stacking-context'></div>
        <div id='stacked-child-of-composited-non-stacking-context'
    style='position: relative'></div>
        <div
    id='non-stacked-layered-child-of-composited-non-stacking-context'
    style='overflow: scroll'></div>
      </div>
    </div>
  )HTML");

  GetDocument().View()->SetTracksPaintInvalidations(true);
  ObjectPaintInvalidator(*GetLayoutObjectByElementId("container"))
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_THAT(*GetDocument().View()->TrackedObjectPaintInvalidations(),
              ElementsAre(
                  PaintInvalidation{
                      GetLayoutObjectByElementId("container")->DebugName(),
                      PaintInvalidationReason::kSubtree},
                  PaintInvalidation{
                      GetLayoutObjectByElementId("normal-child")->DebugName(),
                      PaintInvalidationReason::kSubtree},
                  PaintInvalidation{
                      GetLayoutObjectByElementId("stacked-child")->DebugName(),
                      PaintInvalidationReason::kSubtree},
                  PaintInvalidation{
                      GetLayoutObjectByElementId(
                          "stacked-child-of-composited-non-stacking-context")
                          ->DebugName(),
                      PaintInvalidationReason::kSubtree}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // TODO(crbug.com/922645): This test fails with LayoutNG.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative'>
        <span id='span' style='position: relative; will-change: transform'>
          <div id='target' style='float: right'></div>
        </span>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto* containing_block = GetLayoutObjectByElementId("containingBlock");
  auto* containing_block_layer =
      ToLayoutBoxModelObject(containing_block)->Layer();
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      ToLayoutBoxModelObject(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();

  // Thought |target| is under |span| which is a composited stacking context,
  // |span| is not the paint invalidation container of |target|.
  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_EQ(composited_container, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(containing_block_layer, target->PaintingLayer());

  // Traversing from target should mark needsRepaint on correct layers.
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*target)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());

  UpdateAllLifecyclePhasesForTest();

  // Traversing from span should mark needsRepaint on correct layers for target.
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_TRUE(span_layer->NeedsRepaint());

  UpdateAllLifecyclePhasesForTest();

  // Traversing from compositedContainer should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());

  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{composited_container->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{containing_block->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{target->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{"LayoutText #text",
                                    PaintInvalidationReason::kSubtree}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(ObjectPaintInvalidatorTest,
       TraverseFloatUnderMultiLevelCompositedInlines) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // TODO(crbug.com/922645): This test fails with LayoutNG.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='position: relative; will-change: transform'>
          <span id='innerSpan'
              style='position: relative; will-change: transform'>
            <div id='target' style='float: right'></div>
          </span>
        </span>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto* containing_block = GetLayoutObjectByElementId("containingBlock");
  auto* containing_block_layer =
      ToLayoutBoxModelObject(containing_block)->Layer();
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      ToLayoutBoxModelObject(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();
  auto* inner_span = GetLayoutObjectByElementId("innerSpan");
  auto* inner_span_layer = ToLayoutBoxModelObject(inner_span)->Layer();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_TRUE(inner_span->IsPaintInvalidationContainer());
  EXPECT_TRUE(inner_span->StyleRef().IsStackingContext());
  EXPECT_EQ(composited_container, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(containing_block_layer, target->PaintingLayer());

  // Traversing from compositedContainer should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());
  EXPECT_FALSE(inner_span_layer->NeedsRepaint());

  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{composited_container->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{containing_block->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{target->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{"LayoutText #text",
                                    PaintInvalidationReason::kSubtree}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseStackedFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // TODO(crbug.com/922645): This test fails with LayoutNG.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <span id='span' style='position: relative; will-change: transform'>
      <div id='target' style='position: relative; float: right'></div>
    </span>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto* target_layer = ToLayoutBoxModelObject(target)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_EQ(span, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(target_layer, target->PaintingLayer());

  // Traversing from span should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(span_layer->NeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(span_layer->NeedsRepaint());

  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{span->DebugName(),
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{"LayoutText #text",
                                    PaintInvalidationReason::kSubtree},
                  PaintInvalidation{target->DebugName(),
                                    PaintInvalidationReason::kSubtree}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(ObjectPaintInvalidatorTest, InvalidatePaintRectangle) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; background: blue'>"
      "</div>");

  GetDocument().View()->SetTracksPaintInvalidations(true);

  auto* target = GetLayoutObjectByElementId("target");
  target->InvalidatePaintRectangle(PhysicalRect(10, 10, 50, 50));
  EXPECT_EQ(PhysicalRect(10, 10, 50, 50),
            target->PartialInvalidationLocalRect());
  target->InvalidatePaintRectangle(PhysicalRect(30, 30, 60, 60));
  EXPECT_EQ(PhysicalRect(10, 10, 80, 80),
            target->PartialInvalidationLocalRect());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalRect(), target->PartialInvalidationLocalRect());
  EXPECT_EQ(IntRect(18, 18, 80, 80), target->PartialInvalidationVisualRect());

  target->InvalidatePaintRectangle(PhysicalRect(30, 30, 50, 80));
  EXPECT_EQ(PhysicalRect(30, 30, 50, 80),
            target->PartialInvalidationLocalRect());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // PartialInvalidationVisualRect should accumulate until painting.
  EXPECT_EQ(IntRect(18, 18, 80, 100), target->PartialInvalidationVisualRect());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(PhysicalRect(), target->PartialInvalidationLocalRect());
  EXPECT_EQ(IntRect(), target->PartialInvalidationVisualRect());

  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{target->DebugName(),
                                    PaintInvalidationReason::kRectangle},
                  PaintInvalidation{target->DebugName(),
                                    PaintInvalidationReason::kRectangle}));

  const auto& raster_invalidations = GetLayoutView()
                                         .Layer()
                                         ->GraphicsLayerBacking()
                                         ->GetRasterInvalidationTracking()
                                         ->Invalidations();
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(18, 18, 80, 100), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kRectangle,
            raster_invalidations[0].reason);

  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(ObjectPaintInvalidatorTest, Selection) {
  SetBodyInnerHTML("<img id='target' style='width: 100px; height: 100px'>");
  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(IntRect(), target->SelectionVisualRect());

  // Add selection.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const auto* graphics_layer = GetLayoutView().Layer()->GraphicsLayerBacking();
  const auto* invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  EXPECT_EQ(IntRect(8, 8, 100, 100), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Simulate a change without full invalidation or selection change.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->SetShouldCheckForPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(graphics_layer->GetRasterInvalidationTracking()
                  ->Invalidations()
                  .IsEmpty());
  EXPECT_EQ(IntRect(8, 8, 100, 100), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Remove selection.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().GetFrame()->Selection().Clear();
  UpdateAllLifecyclePhasesForTest();
  invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  EXPECT_EQ(IntRect(), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

}  // namespace blink
