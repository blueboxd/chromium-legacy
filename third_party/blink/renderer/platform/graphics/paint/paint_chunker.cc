// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

namespace blink {

PaintChunker::PaintChunker()
    : current_properties_(PropertyTreeState::Uninitialized()),
      force_new_chunk_(true) {}

PaintChunker::~PaintChunker() = default;

#if DCHECK_IS_ON()
bool PaintChunker::IsInInitialState() const {
  if (current_properties_ != PropertyTreeState::Uninitialized())
    return false;

  DCHECK(chunks_.IsEmpty());
  return true;
}
#endif

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id* chunk_id,
    const PropertyTreeState& properties) {
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
                               item.IsScrollHitTest() || item.IsScrollbar();
  if (item_forces_new_chunk)
    SetForceNewChunk(true);

  auto previous_size = size();
  auto& chunk = EnsureCurrentChunk(item.GetId());
  bool created_new_chunk = size() > previous_size;

  chunk.bounds.Unite(item.VisualRect());
  if (item.DrawsContent())
    chunk.drawable_bounds.Unite(item.VisualRect());
  chunk.outset_for_raster_effects =
      std::max(chunk.outset_for_raster_effects, item.OutsetForRasterEffects());
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

Vector<PaintChunk> PaintChunker::ReleasePaintChunks() {
  next_chunk_id_ = base::nullopt;
  current_properties_ = PropertyTreeState::Uninitialized();
  chunks_.ShrinkToFit();
  return std::move(chunks_);
}

}  // namespace blink
