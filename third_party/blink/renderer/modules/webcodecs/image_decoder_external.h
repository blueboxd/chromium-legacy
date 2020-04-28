// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ImageBitmapOptions;
class ImageDecoder;
class ImageDecoderInit;
class ReadableStreamBytesConsumer;
class ScriptPromiseResolver;
class SegmentReader;

class MODULES_EXPORT ImageDecoderExternal final : public ScriptWrappable,
                                                  public BytesConsumer::Client {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ImageDecoderExternal);

 public:
  static ImageDecoderExternal* Create(ScriptState*,
                                      const ImageDecoderInit*,
                                      ExceptionState&);

  ImageDecoderExternal(ScriptState*, const ImageDecoderInit*, ExceptionState&);
  ~ImageDecoderExternal() override;

  // image_decoder.idl implementation.
  ScriptPromise decode(uint32_t frame_index);
  uint32_t frameCount() const;
  String type() const;
  uint32_t repetitionCount() const;

  // BytesConsumer::Client implementation.
  void OnStateChange() override;
  String DebugName() const override;

  // GarbageCollected override.
  void Trace(Visitor*) override;

 private:
  void MaybeSatisfyPendingDecodes();
  void MaybeCreateImageDecoder(scoped_refptr<SegmentReader> sr);
  void UpdateFrameAndRepetitionCount();

  Member<ScriptState> script_state_;

  // Used when a ReadableStream is provided.
  Member<ReadableStreamBytesConsumer> consumer_;
  scoped_refptr<SharedBuffer> stream_buffer_;

  // Construction parameters.
  Member<const ImageDecoderInit> init_data_;
  Member<ImageBitmapOptions> options_;

  bool data_complete_ = false;

  std::unique_ptr<ImageDecoder> decoder_;
  String mime_type_;
  uint32_t frame_count_ = 0u;
  uint32_t repetition_count_ = 0u;

  // Pending decode() requests.
  struct DecodeRequest : public GarbageCollected<DecodeRequest> {
    DecodeRequest(ScriptPromiseResolver* resolver, uint32_t frame_index);
    void Trace(Visitor*);

    Member<ScriptPromiseResolver> resolver;
    uint32_t frame_index;
    bool complete = false;
  };
  HeapVector<Member<DecodeRequest>> pending_decodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_
