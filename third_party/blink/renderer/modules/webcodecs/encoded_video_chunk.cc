// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

EncodedVideoChunk* EncodedVideoChunk::Create(String type,
                                             uint64_t timestamp,
                                             const DOMArrayPiece& data) {
  return EncodedVideoChunk::Create(type, timestamp, 0 /* duration */, data);
}

EncodedVideoChunk* EncodedVideoChunk::Create(String type,
                                             uint64_t timestamp,
                                             uint64_t duration,
                                             const DOMArrayPiece& data) {
  EncodedVideoMetadata metadata;
  metadata.timestamp = base::TimeDelta::FromMicroseconds(timestamp);
  metadata.key_frame = (type == "key");
  if (duration)
    metadata.duration = base::TimeDelta::FromMicroseconds(duration);
  return MakeGarbageCollected<EncodedVideoChunk>(
      metadata, DOMArrayBuffer::Create(data.Bytes(), data.ByteLengthAsSizeT()));
}

EncodedVideoChunk::EncodedVideoChunk(EncodedVideoMetadata metadata,
                                     DOMArrayBuffer* buffer)
    : metadata_(metadata), buffer_(buffer) {}

String EncodedVideoChunk::type() const {
  return metadata_.key_frame ? "key" : "delta";
}

uint64_t EncodedVideoChunk::timestamp() const {
  return metadata_.timestamp.InMicroseconds();
}

base::Optional<uint64_t> EncodedVideoChunk::duration() const {
  if (!metadata_.duration)
    return base::nullopt;
  return metadata_.duration->InMicroseconds();
}

uint64_t EncodedVideoChunk::duration(bool* is_null) const {
  if (!metadata_.duration) {
    *is_null = true;
    return 0;
  }
  *is_null = false;
  return metadata_.duration->InMicroseconds();
}

DOMArrayBuffer* EncodedVideoChunk::data() const {
  return buffer_;
}

}  // namespace blink
