// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// For PaintArtifact::AppendDebugDrawing().
class DebugDrawingClient final : public DisplayItemClient {
 public:
  DebugDrawingClient() { Invalidate(PaintInvalidationReason::kUncacheable); }
  String DebugName() const final { return "DebugDrawing"; }
  IntRect VisualRect() const final { return LayoutRect::InfiniteIntRect(); }
};

}  // namespace

PaintArtifact::PaintArtifact() : display_item_list_(0) {}

PaintArtifact::PaintArtifact(DisplayItemList display_items,
                             Vector<PaintChunk> chunks)
    : display_item_list_(std::move(display_items)), chunks_(std::move(chunks)) {
}

PaintArtifact::~PaintArtifact() = default;

scoped_refptr<PaintArtifact> PaintArtifact::Create(
    DisplayItemList display_items,
    Vector<PaintChunk> chunks) {
  return base::AdoptRef(
      new PaintArtifact(std::move(display_items), std::move(chunks)));
}

scoped_refptr<PaintArtifact> PaintArtifact::Empty() {
  DEFINE_STATIC_REF(PaintArtifact, empty, base::AdoptRef(new PaintArtifact()));
  return empty;
}

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  size_t total_size = sizeof(*this) + display_item_list_.MemoryUsageInBytes() +
                      chunks_.capacity() * sizeof(chunks_[0]);
  for (const auto& chunk : chunks_)
    total_size += chunk.MemoryUsageInBytes();
  return total_size;
}

void PaintArtifact::AppendDebugDrawing(
    sk_sp<const PaintRecord> record,
    const PropertyTreeState& property_tree_state) {
  DEFINE_STATIC_LOCAL(DebugDrawingClient, debug_drawing_client, ());

  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  auto& display_item =
      display_item_list_.AllocateAndConstruct<DrawingDisplayItem>(
          debug_drawing_client, DisplayItem::kDebugDrawing, std::move(record));

  // Create a PaintChunk for the debug drawing.
  chunks_.emplace_back(display_item_list_.size() - 1, display_item_list_.size(),
                       display_item.GetId(), property_tree_state);
  chunks_.back().bounds = chunks_.back().drawable_bounds =
      display_item_list_.Last().VisualRect();
}

void PaintArtifact::Replay(GraphicsContext& graphics_context,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  Replay(*graphics_context.Canvas(), replay_state, offset);
}

void PaintArtifact::Replay(cc::PaintCanvas& canvas,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  canvas.drawPicture(GetPaintRecord(replay_state, offset));
}

sk_sp<PaintRecord> PaintArtifact::GetPaintRecord(
    const PropertyTreeState& replay_state,
    const IntPoint& offset) const {
  return PaintChunksToCcLayer::Convert(
             PaintChunks(), replay_state,
             gfx::Vector2dF(offset.X(), offset.Y()), GetDisplayItemList(),
             cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
      ->ReleaseAsRecord();
}

SkColor PaintArtifact::SafeOpaqueBackgroundColor(
    const PaintChunkSubset& chunks) const {
  // Find the background color from the first drawing display item.
  for (const auto& chunk : chunks) {
    for (const auto& item : display_item_list_.ItemsInPaintChunk(chunk)) {
      if (item.IsDrawing() && item.DrawsContent())
        return static_cast<const DrawingDisplayItem&>(item).BackgroundColor();
    }
  }
  return SK_ColorTRANSPARENT;
}

void PaintArtifact::FinishCycle() {
  // Until CompositeAfterPaint, PaintController::ClearPropertyTreeChangedStateTo
  // is used for clearing the property tree changed state at the end of paint
  // instead of in FinishCycle. See: LocalFrameView::RunPaintLifecyclePhase.
  bool clear_property_tree_changed =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled();
  for (auto& chunk : chunks_) {
    chunk.client_is_just_created = false;
    if (clear_property_tree_changed)
      chunk.properties.ClearChangedToRoot();
  }
}

}  // namespace blink
