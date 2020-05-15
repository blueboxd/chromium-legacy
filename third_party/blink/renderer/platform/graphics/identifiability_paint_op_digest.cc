// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/identifiability_paint_op_digest.h"

#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_participation.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace blink {

// Storage for serialized PaintOp state.
Vector<char>& SerializationBuffer() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Vector<char>>,
                                  serialization_buffer, ());
  return *serialization_buffer;
}

IdentifiabilityPaintOpDigest::IdentifiabilityPaintOpDigest(IntSize size)
    : size_(size),
      paint_cache_(cc::ClientPaintCache::kNoCachingBudget),
      nodraw_canvas_(size_.Width(), size_.Height()),
      serialize_options_(&image_provider_,
                         /*transfer_cache=*/nullptr,
                         &paint_cache_,
                         &nodraw_canvas_,
                         /*strike_server=*/nullptr,
                         /*color_space=*/nullptr,
                         /*can_use_lcd_text=*/false,
                         /*content_supports_distance_field_text=*/false,
                         /*max_texture_size=*/0,
                         /*original_ctm=*/SkMatrix::I()) {
  constexpr size_t kInitialSize = 16 * 1024;
  if (IsUserInIdentifiabilityStudy() &&
      SerializationBuffer().size() < kInitialSize)
    SerializationBuffer().resize(kInitialSize);
}

IdentifiabilityPaintOpDigest::~IdentifiabilityPaintOpDigest() = default;

constexpr size_t IdentifiabilityPaintOpDigest::kInfiniteOps;

void IdentifiabilityPaintOpDigest::MaybeUpdateDigest(
    const sk_sp<const cc::PaintRecord>& paint_record,
    const size_t num_ops_to_visit) {
  // To minimize performance impact, don't exceed kMaxDigestOps during the
  // lifetime of this IdentifiabilityPaintOpDigest object.
  constexpr int kMaxDigestOps = 1 << 20;
  if (!IsUserInIdentifiabilityStudy() || total_ops_digested_ > kMaxDigestOps)
    return;

  // Determine how many PaintOps we'll need to digest after the initial digests
  // that are skipped.
  const size_t num_ops_to_digest = num_ops_to_visit - prefix_skip_count_;

  // The number of PaintOps digested in this MaybeUpdateDigest() call.
  size_t cur_ops_digested = 0;
  for (const auto* op : cc::PaintRecord::Iterator(paint_record.get())) {
    // Skip initial PaintOps that don't correspond to context operations.
    if (prefix_skip_count_ > 0) {
      prefix_skip_count_--;
      continue;
    }
    // Update the digest for at most |num_ops_to_digest| operations in this
    // MaybeUpdateDigest() invocation.
    if (num_ops_to_visit != kInfiniteOps &&
        cur_ops_digested >= num_ops_to_digest)
      break;

    // To capture font fallback identifiability, we capture text draw operations
    // at the 2D context layer.
    if (op->GetType() == cc::PaintOpType::DrawTextBlob)
      continue;

    // DrawRecord PaintOps contain nested PaintOps.
    if (op->GetType() == cc::PaintOpType::DrawRecord) {
      const auto* draw_record_op = static_cast<const cc::DrawRecordOp*>(op);
      MaybeUpdateDigest(draw_record_op->record, kInfiniteOps);
      continue;
    }

    size_t serialized_size;
    while ((serialized_size = op->Serialize(SerializationBuffer().data(),
                                            SerializationBuffer().size(),
                                            serialize_options_)) == 0) {
      constexpr size_t kMaxBufferSize =
          gpu::raster::RasterInterface::kDefaultMaxOpSizeHint << 2;
      if (SerializationBuffer().size() >= kMaxBufferSize)
        return;
      SerializationBuffer().Grow(SerializationBuffer().size() << 1);
    }
    digest_ ^= IdentifiabilityDigestOfBytes(base::as_bytes(
        base::make_span(SerializationBuffer().data(), serialized_size)));
    total_ops_digested_++;
    cur_ops_digested++;
  }
  DCHECK_EQ(prefix_skip_count_, 0u);
}

cc::ImageProvider::ScopedResult
IdentifiabilityPaintOpDigest::IdentifiabilityImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  // TODO(crbug.com/973801): Compute digests on images.
  return ScopedResult();
}

}  // namespace blink
