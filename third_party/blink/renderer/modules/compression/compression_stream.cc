// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/compression_stream.h"

#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

CompressionStream* CompressionStream::Create(ScriptState* script_state,
                                             const AtomicString& format,
                                             ExceptionState& exception_state) {
  return MakeGarbageCollected<CompressionStream>(script_state, format,
                                                 exception_state);
}

CompressionStream::~CompressionStream() = default;

ReadableStream* CompressionStream::readable() const {
  return transform_->Readable();
}

WritableStream* CompressionStream::writable() const {
  return transform_->Writable();
}

void CompressionStream::Trace(Visitor* visitor) {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

CompressionStream::CompressionStream(ScriptState* script_state,
                                     const AtomicString& format,
                                     ExceptionState& exception_state)
    : transform_(MakeGarbageCollected<TransformStream>()) {
  DeflateTransformer::Format deflate_format;
  if (format == "gzip") {
    deflate_format = DeflateTransformer::Format::Gzip;
  } else if (format == "deflate") {
    deflate_format = DeflateTransformer::Format::Deflate;
  } else {
    exception_state.ThrowTypeError("Unsupported format");
    return;
  }

  // default level is hardcoded for now.
  // TODO(arenevier): Make level configurable
  const int deflate_level = 6;
  transform_->Init(MakeGarbageCollected<DeflateTransformer>(
                       script_state, deflate_format, deflate_level),
                   script_state, exception_state);
}

}  // namespace blink
