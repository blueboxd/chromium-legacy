// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_VIDEO_CHUNK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_VIDEO_CHUNK_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_metadata.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ArrayBuffer;
class DOMArrayBuffer;

class MODULES_EXPORT EncodedVideoChunk final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  EncodedVideoChunk(EncodedVideoMetadata metadata,
                    scoped_refptr<ArrayBuffer> buffer);

  static EncodedVideoChunk* Create(String type,
                                   uint64_t timestamp,
                                   const DOMArrayPiece& data);
  static EncodedVideoChunk* Create(String type,
                                   uint64_t timestamp,
                                   uint64_t duration,
                                   const DOMArrayPiece& data);

  // encoded_video_chunk.idl implementation.
  String type() const;
  uint64_t timestamp() const;
  uint64_t duration(bool* is_null) const;
  uint64_t duration(bool& is_null) const { return duration(&is_null); }
  DOMArrayBuffer* data() const;

 private:
  EncodedVideoMetadata metadata_;
  scoped_refptr<ArrayBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_VIDEO_CHUNK_H_
