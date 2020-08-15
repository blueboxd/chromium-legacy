// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

namespace blink {

PaintChunker::PaintChunker()
    : current_properties_(PropertyTreeState::Uninitialized()),
      force_new_chunk_(true) {}

PaintChunker::~PaintChunker() = default;

#if DCHECK_IS_ON()
bool PaintChunker::IsInInitialState() const {
  if (current_properties_ != PropertyTreeState::Uninitialized())
    return false;
  DCHECK_EQ(candidate_background_color_.Rgb(), Color::kTransparent);
  DCHECK_EQ(candidate_background_area_, 0u);
  DCHECK(chunks_.IsEmpty());
  return true;
}
#endif

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id* chunk_id,
    const PropertyTreeStateOrAlias& properties) {
  // If properties are the same, continue to use the previously set
  // |next_chunk_id_| because the id of the outer painting is likely to be
  // more stable to reduce invalidation because of chunk id changes.
  if (!next_chunk_id_ || current_properties_ != properties) {
    if (chunk_id)
      next_chunk_id_.emplace(*chunk_id);
    else
      next_chunk_id_ = base::nullopt;
  }
  current_properties_ = properties;
}

void PaintChunker::AppendByMoving(PaintChunk&& chunk) {
  FinalizeLastChunkProperties();
  wtf_size_t next_chunk_begin_index =
      chunks_.IsEmpty() ? 0 : LastChunk().end_index;
  chunks_.emplace_back(next_chunk_begin_index, std::move(chunk));
}

PaintChunk& PaintChunker::EnsureCurrentChunk(const PaintChunk::Id& id) {
#if DCHECK_IS_ON()
  // If this DCHECKs are hit we are missing a call to update the properties.
  // See: ScopedPaintChunkProperties.
  DCHECK(!IsInInitialState());
  // At this point we should have all of the properties given to us.
  DCHECK(current_properties_.IsInitialized());
#endif

  if (WillForceNewChunk() || current_properties_ != LastChunk().properties) {
    if (!next_chunk_id_)
      next_chunk_id_.emplace(id);
    FinalizeLastChunkProperties();
    wtf_size_t begin = chunks_.IsEmpty() ? 0 : LastChunk().end_index;
    chunks_.emplace_back(begin, begin, *next_chunk_id_, current_properties_);
    next_chunk_id_ = base::nullopt;
    force_new_chunk_ = false;
  }
  return LastChunk();
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItem& item) {
  bool item_forces_new_chunk = item.IsForeignLayer() ||
                               item.IsGraphicsLayerWrapper() ||
                               item.IsScrollbar();
  if (item_forces_new_chunk)
    SetForceNewChunk(true);

  auto previous_size = size();
  auto& chunk = EnsureCurrentChunk(item.GetId());
  bool created_new_chunk = size() > previous_size;

  chunk.bounds.Unite(item.VisualRect());
  if (item.DrawsContent())
    chunk.drawable_bounds.Unite(item.VisualRect());

  // If this paints the background and it's larger than our current candidate,
  // set the candidate to be this item.
  if (item.IsDrawing() && item.DrawsContent()) {
    uint64_t item_area;
    Color item_color =
        static_cast<const DrawingDisplayItem&>(item).BackgroundColor(item_area);
    ProcessBackgroundColorCandidate(chunk.id, item_color, item_area);
  }

  constexpr wtf_size_t kMaxRegionComplexity = 10;
  if (item.IsDrawing() &&
      static_cast<const DrawingDisplayItem&>(item).KnownToBeOpaque() &&
      last_chunk_known_to_be_opaque_region_.Complexity() < kMaxRegionComplexity)
    last_chunk_known_to_be_opaque_region_.Unite(item.VisualRect());

  chunk.raster_effect_outset =
      std::max(chunk.raster_effect_outset, item.GetRasterEffectOutset());

  chunk.end_index++;

  // When forcing a new chunk, we still need to force new chunk for the next
  // display item. Otherwise reset force_new_chunk_ to false.
  DCHECK(!force_new_chunk_);
  if (item_forces_new_chunk) {
    DCHECK(created_new_chunk);
    SetForceNewChunk(true);
  }

  return created_new_chunk;
}

void PaintChunker::AddHitTestDataToCurrentChunk(const PaintChunk::Id& id,
                                                const IntRect& rect,
                                                TouchAction touch_action) {
  // In CompositeAfterPaint, we ensure a paint chunk for correct composited
  // hit testing. In pre-CompositeAfterPaint, this is unnecessary, except that
  // there is special touch action, and that we have a non-root effect so that
  // PaintChunksToCcLayer will emit paint operations for filters.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      touch_action == TouchAction::kAuto &&
      &current_properties_.Effect() == &EffectPaintPropertyNode::Root())
    return;

  auto& chunk = EnsureCurrentChunk(id);
  chunk.bounds.Unite(rect);
  if (touch_action != TouchAction::kAuto) {
    chunk.EnsureHitTestData().touch_action_rects.push_back(
        TouchActionRect{rect, touch_action});
  }
}

void PaintChunker::CreateScrollHitTestChunk(
    const PaintChunk::Id& id,
    const TransformPaintPropertyNode* scroll_translation,
    const IntRect& rect) {
#if DCHECK_IS_ON()
  if (id.type == DisplayItem::Type::kResizerScrollHitTest ||
      id.type == DisplayItem::Type::kPluginScrollHitTest ||
      id.type == DisplayItem::Type::kCustomScrollbarHitTest) {
    // Resizer and plugin scroll hit tests are only used to prevent composited
    // scrolling and should not have a scroll offset node.
    DCHECK(!scroll_translation);
  } else if (id.type == DisplayItem::Type::kScrollHitTest) {
    DCHECK(scroll_translation);
    // The scroll offset transform node should have an associated scroll node.
    DCHECK(scroll_translation->ScrollNode());
  } else {
    NOTREACHED();
  }
#endif

  SetForceNewChunk(true);
  auto& chunk = EnsureCurrentChunk(id);
  chunk.bounds.Unite(rect);
  auto& hit_test_data = chunk.EnsureHitTestData();
  hit_test_data.scroll_translation = scroll_translation;
  hit_test_data.scroll_hit_test_rect = rect;
  SetForceNewChunk(true);
}

void PaintChunker::ProcessBackgroundColorCandidate(const PaintChunk::Id& id,
                                                   Color color,
                                                   uint64_t area) {
  EnsureCurrentChunk(id);
  if (color != Color::kTransparent &&
      (candidate_background_color_ == Color::kTransparent ||
       (area >= candidate_background_area_))) {
    candidate_background_color_ = color;
    candidate_background_area_ = area;
  }
}

void PaintChunker::FinalizeLastChunkProperties() {
  if (chunks_.IsEmpty() || LastChunk().is_moved_from_cached_subsequence)
    return;

  LastChunk().known_to_be_opaque =
      last_chunk_known_to_be_opaque_region_.Contains(LastChunk().bounds);
  last_chunk_known_to_be_opaque_region_ = Region();

  if (candidate_background_color_ != Color::kTransparent) {
    LastChunk().background_color = candidate_background_color_;
    LastChunk().background_color_area = candidate_background_area_;
  }
  candidate_background_color_ = Color::kTransparent;
  candidate_background_area_ = 0u;
}

Vector<PaintChunk> PaintChunker::ReleasePaintChunks() {
  FinalizeLastChunkProperties();
  next_chunk_id_ = base::nullopt;
  current_properties_ = PropertyTreeState::Uninitialized();
  chunks_.ShrinkToFit();
  force_new_chunk_ = true;
  return std::move(chunks_);
}

}  // namespace blink
