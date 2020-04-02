// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"

namespace blink {

MemoryManagedPaintCanvas::MemoryManagedPaintCanvas(
    cc::DisplayItemList* list,
    const SkRect& bounds,
    base::RepeatingClosure set_needs_flush_callback)
    : RecordPaintCanvas(list, bounds),
      set_needs_flush_callback_(std::move(set_needs_flush_callback)) {}

MemoryManagedPaintCanvas::~MemoryManagedPaintCanvas() = default;

void MemoryManagedPaintCanvas::drawImage(const cc::PaintImage& image,
                                         SkScalar left,
                                         SkScalar top,
                                         const cc::PaintFlags* flags) {
  DCHECK(!image.IsPaintWorklet());
  RecordPaintCanvas::drawImage(image, left, top, flags);
  UpdateMemoryUsage(image);
}

void MemoryManagedPaintCanvas::drawImageRect(
    const cc::PaintImage& image,
    const SkRect& src,
    const SkRect& dst,
    const cc::PaintFlags* flags,
    PaintCanvas::SrcRectConstraint constraint) {
  RecordPaintCanvas::drawImageRect(image, src, dst, flags, constraint);
  UpdateMemoryUsage(image);
}

void MemoryManagedPaintCanvas::UpdateMemoryUsage(const cc::PaintImage& image) {
  if (cached_image_ids_.contains(image.GetContentIdForFrame(0u)))
    return;

  cached_image_ids_.insert(image.GetContentIdForFrame(0u));
  total_stored_image_memory_ +=
      image.GetSkImage()->imageInfo().computeMinByteSize();

  if (total_stored_image_memory_ > kMaxPinnedMemory)
    set_needs_flush_callback_.Run();
}

}  // namespace blink
