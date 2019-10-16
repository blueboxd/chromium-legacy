// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
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
      if (record_op->HasText())
        ParseGlyphs(record_op->record.get(), tracker);
    }
  }
}

bool SerializeAsSkPicture(sk_sp<const cc::PaintRecord> record,
                          PaintPreviewTracker* tracker,
                          const gfx::Rect& dimensions,
                          base::File file) {
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
  FileWStream stream(std::move(file));
  skp->serialize(&stream, &serial_procs);
  stream.flush();
  stream.Close();
  return true;
}

bool BuildAndSerializeProto(PaintPreviewTracker* tracker,
                            base::ReadOnlySharedMemoryRegion* region) {
  PaintPreviewFrameProto proto;
  proto.set_unguessable_token_high(tracker->Guid().GetHighForSerialization());
  proto.set_unguessable_token_low(tracker->Guid().GetLowForSerialization());
  proto.set_id(tracker->RoutingId());
  proto.set_is_main_frame(tracker->IsMainFrame());

  auto* proto_content_proxy_map = proto.mutable_content_id_proxy_id_map();
  for (const auto& id_pair : *(tracker->GetPictureSerializationContext()))
    proto_content_proxy_map->insert(
        google::protobuf::MapPair<uint32_t, int64_t>(id_pair.first,
                                                     id_pair.second));
  for (const auto& link : tracker->GetLinks())
    *proto.add_links() = link;

  base::MappedReadOnlyRegion region_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(proto.ByteSizeLong());
  if (!region_mapping.IsValid())
    return false;
  bool success = proto.SerializeToArray(region_mapping.mapping.memory(),
                                        proto.ByteSizeLong());
  if (success)
    *region = std::move(region_mapping.region);
  return success;
}

}  // namespace paint_preview
