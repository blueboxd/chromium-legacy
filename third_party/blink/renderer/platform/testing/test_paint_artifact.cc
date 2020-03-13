// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

#include <memory>
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

sk_sp<PaintRecord> DummyRectClient::MakeRecord(const IntRect& rect,
                                               Color color) {
  rect_ = rect;
  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording(rect);
  PaintFlags flags;
  flags.setColor(color.Rgb());
  canvas->drawRect(rect, flags);
  return recorder.finishRecordingAsPicture();
}

TestPaintArtifact::TestPaintArtifact() : display_item_list_(0) {}

TestPaintArtifact::~TestPaintArtifact() = default;

static DummyRectClient& StaticDummyClient() {
  DEFINE_STATIC_LOCAL(DummyRectClient, client, ());
  client.Validate();
  return client;
}

TestPaintArtifact& TestPaintArtifact::Chunk(int id) {
  Chunk(StaticDummyClient(),
        static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + id));
  // The default bounds with magic numbers make the chunks have different bounds
  // from each other, for e.g. RasterInvalidatorTest to check the tracked raster
  // invalidation rects of chunks. The actual values don't matter. If the chunk
  // has display items, we will recalculate the bounds from the display items
  // when constructing the PaintArtifact.
  Bounds(IntRect(id * 110, id * 220, id * 220 + 200, id * 110 + 200));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Chunk(DummyRectClient& client,
                                            DisplayItem::Type type) {
  paint_chunks_.push_back(
      PaintChunk(display_item_list_.size(), display_item_list_.size(),
                 PaintChunk::Id(client, type), PropertyTreeState::Root()));
  // Assume PaintController has processed this chunk.
  paint_chunks_.back().client_is_just_created = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Properties(
    const PropertyTreeState& properties) {
  paint_chunks_.back().properties = properties;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(const IntRect& bounds,
                                                  Color color) {
  return RectDrawing(NewClient(), bounds, color);
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    const TransformPaintPropertyNode* scroll_offset,
    const IntRect& scroll_container_bounds) {
  return ScrollHitTest(NewClient(), scroll_offset, scroll_container_bounds);
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& offset) {
  DEFINE_STATIC_LOCAL(LiteralDebugNameClient, client, ("ForeignLayer"));
  display_item_list_.AllocateAndConstruct<ForeignLayerDisplayItem>(
      client, DisplayItem::kForeignLayerFirst, std::move(layer), offset,
      nullptr);
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(DummyRectClient& client,
                                                  const IntRect& bounds,
                                                  Color color) {
  display_item_list_.AllocateAndConstruct<DrawingDisplayItem>(
      client, DisplayItem::kDrawingFirst, client.MakeRecord(bounds, color));
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    DummyRectClient& client,
    const TransformPaintPropertyNode* scroll_translation,
    const IntRect& scroll_container_bounds) {
  paint_chunks_.back().EnsureHitTestData().scroll_translation =
      scroll_translation;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::OutsetForRasterEffects(float outset) {
  paint_chunks_.back().outset_for_raster_effects = outset;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::KnownToBeOpaque() {
  paint_chunks_.back().known_to_be_opaque = true;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Bounds(const IntRect& bounds) {
  paint_chunks_.back().bounds = bounds;
  paint_chunks_.back().drawable_bounds = bounds;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::DrawableBounds(
    const IntRect& drawable_bounds) {
  paint_chunks_.back().drawable_bounds = drawable_bounds;
  DCHECK(paint_chunks_.back().bounds.Contains(drawable_bounds));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Uncacheable() {
  paint_chunks_.back().is_cacheable = false;
  return *this;
}

scoped_refptr<PaintArtifact> TestPaintArtifact::Build() {
  return PaintArtifact::Create(std::move(display_item_list_),
                               std::move(paint_chunks_));
}

DummyRectClient& TestPaintArtifact::NewClient() {
  dummy_clients_.push_back(std::make_unique<DummyRectClient>());
  return *dummy_clients_.back();
}

DummyRectClient& TestPaintArtifact::Client(wtf_size_t i) const {
  return *dummy_clients_[i];
}

void TestPaintArtifact::DidAddDisplayItem() {
  auto& chunk = paint_chunks_.back();
  DCHECK_EQ(chunk.end_index, display_item_list_.size() - 1);
  const auto& item = display_item_list_.Last();
  chunk.bounds.Unite(item.VisualRect());
  if (item.DrawsContent())
    chunk.drawable_bounds.Unite(item.VisualRect());
  chunk.end_index++;
}

}  // namespace blink
