// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"

#include <string.h>
#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"

#include "v8/include/v8.h"

namespace blink {

DeflateTransformer::DeflateTransformer(ScriptState* script_state,
                                       Format format,
                                       int level)
    : script_state_(script_state),
      // TODO(arenevier): same as PODArena default chunk size. Is that
      // reasonable? Or should we use a variable size buffer?
      buffer_(16384) {
  DCHECK(level >= 1 && level <= 9);
  int window_bits = 15;
  if (format == Format::Gzip) {
    window_bits += 16;
  }
  memset(&stream_, 0, sizeof(z_stream));
  int err = deflateInit2(&stream_, level, Z_DEFLATED, window_bits, 8,
                         Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, err);
}

DeflateTransformer::~DeflateTransformer() {
  if (!is_stream_freed_) {
    deflateEnd(&stream_);
  }
}

void DeflateTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  NotShared<DOMUint8Array> chunk_data = ToNotShared<NotShared<DOMUint8Array>>(
      script_state_->GetIsolate(), chunk, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  if (!chunk_data) {
    exception_state.ThrowTypeError("chunk is not of type Uint8Array.");
    return;
  }

  if (chunk_data.View()->length() == 0) {
    return;
  }

  Deflate(chunk_data.View(), false, controller, exception_state);
}

void DeflateTransformer::Flush(
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  Deflate(nullptr, true, controller, exception_state);
  is_stream_freed_ = true;
  deflateEnd(&stream_);
}

void DeflateTransformer::Deflate(
    const DOMUint8Array* data,
    bool finished,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  if (data) {
    stream_.avail_in = data->length();
    stream_.next_in = data->DataMaybeShared();
  } else {
    stream_.avail_in = 0;
    stream_.next_in = nullptr;
  }

  do {
    stream_.avail_out = buffer_.size();
    stream_.next_out = buffer_.data();
    int err = deflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    DCHECK((finished && err == Z_STREAM_END) || (err == Z_OK));

    wtf_size_t bytes = buffer_.size() - stream_.avail_out;
    if (bytes) {
      controller->Enqueue(
          ToV8(DOMUint8Array::Create(buffer_.data(), bytes), script_state_),
          exception_state);
      if (exception_state.HadException()) {
        return;
      }
    }
  } while (stream_.avail_out == 0);
}

void DeflateTransformer::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
