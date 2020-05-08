// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "components/paint_preview/common/paint_preview_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class NGBoxFragmentPainterTest : public PaintControllerPaintTest,
                                 private ScopedLayoutNGForTest {
 public:
  NGBoxFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(NGBoxFragmentPainterTest);

TEST_P(NGBoxFragmentPainterTest, ScrollHitTestOrder) {
  GetPage().GetSettings().SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 40px;
        height: 40px;
        overflow: scroll;
        font-size: 500px;
      }
    </style>
    <div id='scroller'>TEXT</div>
  )HTML");
  auto& scroller = ToLayoutBox(*GetLayoutObjectByElementId("scroller"));

  const DisplayItemClient& root_fragment =
      scroller.PaintFragment()
          ? static_cast<const DisplayItemClient&>(*scroller.PaintFragment())
          : static_cast<const DisplayItemClient&>(scroller);

  NGInlineCursor cursor;
  cursor.MoveTo(*scroller.SlowFirstChild());
  const DisplayItemClient& text_fragment =
      *cursor.Current().GetDisplayItemClient();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   DisplayItem::kDocumentBackground),
                          IsSameId(&text_fragment, kForegroundType)));
  HitTestData scroll_hit_test;
  scroll_hit_test.scroll_translation =
      &scroller.FirstFragment().ContentsProperties().Transform();
  scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 40, 40);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 0), IsPaintChunk(0, 1),  // LayoutView chunks.
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(*scroller.Layer(), DisplayItem::kLayerChunk),
                scroller.FirstFragment().LocalBorderBoxProperties()),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(root_fragment, DisplayItem::kScrollHitTest),
                scroller.FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test, IntRect(0, 0, 40, 40)),
            IsPaintChunk(1, 2)));
  } else {
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 1),  // LayutView.
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(root_fragment, DisplayItem::kScrollHitTest),
                scroller.FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test, IntRect(0, 0, 40, 40)),
            IsPaintChunk(1, 2)));
  }
}

TEST_P(NGBoxFragmentPainterTest, AddUrlRects) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div>
      <p>
        <a href="https://www.chromium.org">Chromium</a>
      </p>
      <p>
        <a href="https://www.wikipedia.org">Wikipedia</a>
      </p>
    </div>
  )HTML");
  // Use Paint Preview to test this as printing falls back to the legacy layout
  // engine.

  // PaintPreviewTracker records URLs via the GraphicsContext under certain
  // flagsets when painting. This is the simplest way to check if URLs were
  // annotated.
  GetDocument().SetIsPaintingPreview(true);
  UpdateAllLifecyclePhasesForTest();

  paint_preview::PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                                             base::nullopt, true);
  PaintRecordBuilder builder(nullptr, nullptr, nullptr, &tracker);
  builder.Context().SetIsPaintingPreview(true);

  GetDocument().View()->PaintContentsOutsideOfLifecycle(
      builder.Context(),
      kGlobalPaintNormalPhase | kGlobalPaintAddUrlMetadata |
          kGlobalPaintFlattenCompositingLayers,
      CullRect::Infinite());

  builder.EndRecording();
  ASSERT_EQ(tracker.GetLinks().size(), 2U);
  EXPECT_EQ(tracker.GetLinks()[0]->url, "https://www.chromium.org/");
  EXPECT_EQ(tracker.GetLinks()[1]->url, "https://www.wikipedia.org/");
}

}  // namespace blink
