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
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

void ParseGlyphsAndLinks(const cc::PaintOpBuffer* buffer,
                         PaintPreviewTracker* tracker) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    switch (it->GetType()) {
      case cc::PaintOpType::DrawTextBlob: {
        auto* text_blob_op = static_cast<cc::DrawTextBlobOp*>(*it);
        tracker->AddGlyphs(text_blob_op->blob.get());
        break;
      }
      case cc::PaintOpType::DrawRecord: {
        // Recurse into nested records if they contain text blobs (equivalent to
        // nested SkPictures).
        auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
        ParseGlyphsAndLinks(record_op->record.get(), tracker);
        break;
      }
      case cc::PaintOpType::Annotate: {
        auto* annotate_op = static_cast<cc::AnnotateOp*>(*it);
        tracker->AnnotateLink(GURL(std::string(reinterpret_cast<const char*>(
                                                   annotate_op->data->data()),
                                               annotate_op->data->size())),
                              annotate_op->rect);
        // Delete the data. We no longer need it.
        annotate_op->data.reset();
        break;
      }
      case cc::PaintOpType::CustomData: {
        auto* custom_op = static_cast<cc::CustomDataOp*>(*it);
        custom_op->id = tracker->TransformContentForRemoteFrame(custom_op->id);
        break;
      }
      case cc::PaintOpType::Save: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::SaveLayer: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::SaveLayerAlpha: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::Restore: {
        tracker->Restore();
        break;
      }
      case cc::PaintOpType::SetMatrix: {
        auto* matrix_op = static_cast<cc::SetMatrixOp*>(*it);
        tracker->SetMatrix(matrix_op->matrix);
        break;
      }
      case cc::PaintOpType::Concat: {
        auto* concat_op = static_cast<cc::ConcatOp*>(*it);
        tracker->Concat(concat_op->matrix);
        break;
      }
      case cc::PaintOpType::Scale: {
        auto* scale_op = static_cast<cc::ScaleOp*>(*it);
        tracker->Scale(scale_op->sx, scale_op->sy);
        break;
      }
      case cc::PaintOpType::Rotate: {
        auto* rotate_op = static_cast<cc::RotateOp*>(*it);
        tracker->Rotate(rotate_op->degrees);
        break;
      }
      case cc::PaintOpType::Translate: {
        auto* translate_op = static_cast<cc::TranslateOp*>(*it);
        tracker->Translate(translate_op->dx, translate_op->dy);
        break;
      }
      default:
        continue;
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
