// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by node BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

CompositorElementId ScrollbarElementId(const cc::Scrollbar& scrollbar) {
  return CompositorElementIdFromUniqueObjectId(
      13579, scrollbar.Orientation() == cc::HORIZONTAL
                 ? CompositorElementIdNamespace::kHorizontalScrollbar
                 : CompositorElementIdNamespace::kVerticalScrollbar);
}

CompositorElementId ScrollElementId() {
  return CompositorElementIdFromUniqueObjectId(
      24680, CompositorElementIdNamespace::kScroll);
}

scoped_refptr<TransformPaintPropertyNode> CreateScrollTranslation() {
  ScrollPaintPropertyNode::State state{IntRect(0, 0, 100, 100),
                                       IntSize(1000, 1000)};
  state.compositor_element_id = ScrollElementId();
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(state));
  return CreateScrollTranslation(t0(), 0, 0, *scroll);
}

TEST(ScrollbarDisplayItemTest, HorizontalSolidColorScrollbar) {
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_orientation(cc::HORIZONTAL);
  scrollbar->set_is_solid_color(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_track_rect(gfx::Rect(2, 90, 96, 10));
  scrollbar->set_thumb_size(gfx::Size(30, 7));

  FakeDisplayItemClient client;
  IntRect scrollbar_rect(0, 90, 100, 10);
  auto scroll_translation = CreateScrollTranslation();
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(client, DisplayItem::kScrollbarHorizontal,
                                    scrollbar, scrollbar_rect,
                                    scroll_translation.get(), element_id);
  auto layer = display_item.GetLayer();
  ASSERT_EQ(cc::ScrollbarLayerBase::kSolidColor,
            layer->ScrollbarLayerTypeForTesting());
  auto* scrollbar_layer =
      static_cast<cc::SolidColorScrollbarLayer*>(layer.get());
  EXPECT_EQ(cc::HORIZONTAL, scrollbar_layer->orientation());
  EXPECT_EQ(7, scrollbar_layer->thumb_thickness());
  EXPECT_EQ(2, scrollbar_layer->track_start());
  EXPECT_EQ(element_id, scrollbar_layer->element_id());
  EXPECT_EQ(ScrollElementId(), scrollbar_layer->scroll_element_id());

  EXPECT_EQ(layer.get(), display_item.GetLayer().get());
}

TEST(ScrollbarDisplayItemTest, VerticalSolidColorScrollbar) {
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_orientation(cc::VERTICAL);
  scrollbar->set_is_solid_color(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_track_rect(gfx::Rect(90, 2, 10, 96));
  scrollbar->set_thumb_size(gfx::Size(7, 30));

  FakeDisplayItemClient client;
  IntRect scrollbar_rect(90, 0, 10, 100);
  auto scroll_translation = CreateScrollTranslation();
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(client, DisplayItem::kScrollbarHorizontal,
                                    scrollbar, scrollbar_rect,
                                    scroll_translation.get(), element_id);
  auto layer = display_item.GetLayer();
  ASSERT_EQ(cc::ScrollbarLayerBase::kSolidColor,
            layer->ScrollbarLayerTypeForTesting());
  auto* scrollbar_layer =
      static_cast<cc::SolidColorScrollbarLayer*>(layer.get());
  EXPECT_EQ(cc::VERTICAL, scrollbar_layer->orientation());
  EXPECT_EQ(7, scrollbar_layer->thumb_thickness());
  EXPECT_EQ(2, scrollbar_layer->track_start());
  EXPECT_EQ(element_id, scrollbar_layer->element_id());
  EXPECT_EQ(ScrollElementId(), scrollbar_layer->scroll_element_id());

  EXPECT_EQ(layer.get(), display_item.GetLayer().get());
}

TEST(ScrollbarDisplayItemTest, PaintedColorScrollbar) {
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();

  FakeDisplayItemClient client;
  IntRect scrollbar_rect(0, 90, 100, 10);
  auto scroll_translation = CreateScrollTranslation();
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(client, DisplayItem::kScrollbarHorizontal,
                                    scrollbar, scrollbar_rect,
                                    scroll_translation.get(), element_id);
  auto layer = display_item.GetLayer();
  ASSERT_EQ(cc::ScrollbarLayerBase::kPainted,
            layer->ScrollbarLayerTypeForTesting());

  EXPECT_EQ(layer.get(), display_item.GetLayer().get());
}

TEST(ScrollbarDisplayItemTest, PaintedColorScrollbarOverlayNonNinePatch) {
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_has_thumb(true);
  scrollbar->set_is_overlay(true);

  FakeDisplayItemClient client;
  IntRect scrollbar_rect(0, 90, 100, 10);
  auto scroll_translation = CreateScrollTranslation();
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(client, DisplayItem::kScrollbarHorizontal,
                                    scrollbar, scrollbar_rect,
                                    scroll_translation.get(), element_id);
  auto layer = display_item.GetLayer();
  // We should create PaintedScrollbarLayer instead of
  // PaintedOverlayScrollbarLayer for non-nine-patch overlay scrollbars.
  ASSERT_EQ(cc::ScrollbarLayerBase::kPainted,
            layer->ScrollbarLayerTypeForTesting());

  EXPECT_EQ(layer.get(), display_item.GetLayer().get());
}

TEST(ScrollbarDisplayItemTest, PaintedColorScrollbarOverlayNinePatch) {
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_has_thumb(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_uses_nine_patch_thumb_resource(true);

  FakeDisplayItemClient client;
  IntRect scrollbar_rect(0, 90, 100, 10);
  auto scroll_translation = CreateScrollTranslation();
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(client, DisplayItem::kScrollbarHorizontal,
                                    scrollbar, scrollbar_rect,
                                    scroll_translation.get(), element_id);
  auto layer = display_item.GetLayer();
  ASSERT_EQ(cc::ScrollbarLayerBase::kPaintedOverlay,
            layer->ScrollbarLayerTypeForTesting());

  EXPECT_EQ(layer.get(), display_item.GetLayer().get());
}

}  // namespace blink
