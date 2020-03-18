// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
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
            IsPaintChunk(0, 1,
                         PaintChunk::Id(ViewScrollingBackgroundClient(),
                                        DisplayItem::kDocumentBackground),
                         GetLayoutView().FirstFragment().ContentsProperties()),
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
            IsPaintChunk(0, 1,
                         PaintChunk::Id(ViewScrollingBackgroundClient(),
                                        DisplayItem::kDocumentBackground),
                         GetLayoutView().FirstFragment().ContentsProperties()),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(root_fragment, DisplayItem::kScrollHitTest),
                scroller.FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test, IntRect(0, 0, 40, 40)),
            IsPaintChunk(1, 2)));
  }
}

}  // namespace blink
