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
                          base::File file,
                          size_t max_size,
                          size_t* serialized_size) {
  TRACE_EVENT0("paint_preview", "SerializeAsSkPicture");
  if (!file.IsValid())
    return false;

  // base::Unretained is safe as |tracker| outlives the usage of
  // |custom_callback|.
  cc::PlaybackParams::CustomDataRasterCallback custom_callback =
      base::BindRepeating(&PaintPreviewTracker::CustomDataToSkPictureCallback,
                          base::Unretained(tracker));
  auto skp = ToSkPicture(
      record, SkRect::MakeWH(dimensions.width(), dimensions.height()), nullptr,
      custom_callback);
  if (!skp)
    return false;

  TypefaceSerializationContext typeface_context(tracker->GetTypefaceUsageMap());
  auto serial_procs = MakeSerialProcs(tracker->GetPictureSerializationContext(),
                                      &typeface_context);
  FileWStream stream(std::move(file), max_size);
  skp->serialize(&stream, &serial_procs);
  stream.flush();
  stream.Close();
  DCHECK(serialized_size);
  *serialized_size = stream.ActualBytesWritten();
  return !stream.DidWriteFail();
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

}  // namespace paint_preview
