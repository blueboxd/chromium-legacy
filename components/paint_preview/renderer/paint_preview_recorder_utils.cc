// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

namespace paint_preview {

void ParseGlyphs(const cc::PaintOpBuffer* buffer,
                 PaintPreviewTracker* tracker) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    if (it->GetType() == cc::PaintOpType::DrawTextBlob) {
      auto* text_blob_op = static_cast<cc::DrawTextBlobOp*>(*it);
      tracker->AddGlyphs(text_blob_op->blob.get());
    } else if (it->GetType() == cc::PaintOpType::DrawRecord) {
      // Recurse into nested records if they contain text blobs (equivalent to
      // nested SkPictures).
      auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
      ParseGlyphs(record_op->record.get(), tracker);
    }
  }
}

bool SerializeAsSkPicture(sk_sp<const cc::PaintRecord> record,
                          PaintPreviewTracker* tracker,
                          const gfx::Rect& dimensions,
                          SkWStream* out_stream) {
  TRACE_EVENT0("paint_preview", "SerializeAsSkPicture");

  // base::Unretained is safe as |tracker| outlives the usage of
  // |custom_callback|.
  cc::PlaybackParams::CustomDataRasterCallback custom_callback =
      base::BindRepeating(&PaintPreviewTracker::CustomDataToSkPictureCallback,
                          base::Unretained(tracker));
  auto skp = ToSkPicture(
      record, SkRect::MakeWH(dimensions.width(), dimensions.height()), nullptr,
      custom_callback);
  if (!skp || skp->cullRect().width() == 0 || skp->cullRect().height() == 0)
    return false;

  TypefaceSerializationContext typeface_context(tracker->GetTypefaceUsageMap());
  auto serial_procs = MakeSerialProcs(tracker->GetPictureSerializationContext(),
                                      &typeface_context);

  skp->serialize(out_stream, &serial_procs);
  out_stream->flush();
  return true;
}

void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response) {
  response->embedding_token = tracker->EmbeddingToken();

  PictureSerializationContext* picture_context =
      tracker->GetPictureSerializationContext();
  if (picture_context) {
    for (const auto& id_pair : *picture_context) {
      response->content_id_to_embedding_token.insert(
          {id_pair.first, id_pair.second});
    }
  }

  tracker->MoveLinks(&response->links);
}

bool SerializeAsSkPictureToFile(sk_sp<const cc::PaintRecord> recording,
                                const gfx::Rect& bounds,
                                PaintPreviewTracker* tracker,
                                base::File file,
                                size_t max_capture_size,
                                size_t* serialized_size) {
  if (!file.IsValid())
    return false;

  FileWStream file_stream(std::move(file), max_capture_size);
  if (!SerializeAsSkPicture(recording, tracker, bounds, &file_stream))
    return false;

  file_stream.Close();
  *serialized_size = file_stream.ActualBytesWritten();
  return !file_stream.DidWriteFail();
}

bool SerializeAsSkPictureToMemoryBuffer(sk_sp<const cc::PaintRecord> recording,
                                        const gfx::Rect& bounds,
                                        PaintPreviewTracker* tracker,
                                        mojo_base::BigBuffer* buffer,
                                        size_t max_capture_size,
                                        size_t* serialized_size) {
  SkDynamicMemoryWStream memory_stream;
  if (!SerializeAsSkPicture(recording, tracker, bounds, &memory_stream))
    return false;

  // |0| indicates "no size limit".
  if (max_capture_size == 0)
    max_capture_size = SIZE_MAX;

  sk_sp<SkData> data = memory_stream.detachAsData();
  *serialized_size = std::min(data->size(), max_capture_size);
  *buffer = mojo_base::BigBuffer(
      base::span<const uint8_t>(data->bytes(), *serialized_size));
  return data->size() <= max_capture_size;
}

}  // namespace paint_preview
