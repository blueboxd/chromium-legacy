// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "components/paint_preview/common/subset_font.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace paint_preview {

namespace {

// Serializes a SkPicture representing a subframe as a custom data placeholder.
sk_sp<SkData> SerializeSubframe(SkPicture* picture, void* ctx) {
  const PictureSerializationContext* context =
      reinterpret_cast<PictureSerializationContext*>(ctx);
  uint32_t content_id = picture->uniqueID();
  if (context->count(content_id))
    return SkData::MakeWithCopy(&content_id, sizeof(content_id));
  // Defers picture serialization behavior to Skia.
  return nullptr;
}

// De-duplicates and subsets used typefaces and discards any unused typefaces.
sk_sp<SkData> SerializeTypeface(SkTypeface* typeface, void* ctx) {
  TypefaceSerializationContext* context =
      reinterpret_cast<TypefaceSerializationContext*>(ctx);

  if (context->finished.count(typeface->uniqueID()))
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  context->finished.insert(typeface->uniqueID());

  auto usage_it = context->usage->find(typeface->uniqueID());
  if (usage_it == context->usage->end())
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);

  auto subset_data = SubsetFont(typeface, *usage_it->second);
  // This will fail if the font cannot be subsetted properly. In such cases
  // all typeface data should be added for portability.
  if (!subset_data)
    return typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  return subset_data;
}

// Deserializies a SkPicture within the main SkPicture. These represent
// subframes and require special decoding as they are custom data rather than a
// valid SkPicture.
// Precondition: the version of the SkPicture should be checked prior to
// invocation to ensure deserialization will succeed.
sk_sp<SkPicture> DeserializeSubframe(const void* data,
                                     size_t length,
                                     void* ctx) {
  uint32_t content_id;
  if (length < sizeof(content_id))
    return MakeEmptyPicture();
  memcpy(&content_id, data, sizeof(content_id));
  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  auto it = context->find(content_id);
  if (it == context->end() || !it->second)
    return MakeEmptyPicture();
  return it->second;
}

}  // namespace

TypefaceSerializationContext::TypefaceSerializationContext(
    TypefaceUsageMap* usage)
    : usage(usage) {}
TypefaceSerializationContext::~TypefaceSerializationContext() = default;

sk_sp<SkPicture> MakeEmptyPicture() {
  // Effectively a no-op.
  SkPictureRecorder rec;
  rec.beginRecording(1, 1);
  return rec.finishRecordingAsPicture();
}

SkSerialProcs MakeSerialProcs(PictureSerializationContext* picture_ctx,
                              TypefaceSerializationContext* typeface_ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializeSubframe;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = SerializeTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  // TODO(crbug/1008875): find a consistently smaller and low-memory overhead
  // image downsampling method to use as fImageProc.
  return procs;
}

SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializeSubframe;
  procs.fPictureCtx = ctx;
  return procs;
}

}  // namespace paint_preview
