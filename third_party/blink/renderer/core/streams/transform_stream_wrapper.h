// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_WRAPPER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ReadableStream;
class ScriptState;
class TransformStreamTransformer;
class WritableStream;

// Helpers to create a TransformStream using V8 Extras. This is not a subclass
// of TransformStream, because it is only needed during creation of the object.
class TransformStreamWrapper {
  STATIC_ONLY(TransformStreamWrapper);

 public:
  // Creates a (readable, writable) pair from JS objects.
  static void InitFromJS(ScriptState*,
                         ScriptValue transformer,
                         ScriptValue writable_strategy,
                         ScriptValue readable_strategy,
                         Member<ReadableStream>*,
                         Member<WritableStream>*,
                         ExceptionState&);

  // Creates a (readable, writable) pair from a C++ object.
  static void Init(ScriptState*,
                   TransformStreamTransformer*,
                   Member<ReadableStream>*,
                   Member<WritableStream>*,
                   ExceptionState&);

 private:
  class Algorithm;
  class FlushAlgorithm;
  class TransformAlgorithm;

  // Creates a (readable, writable) pair from an internal V8 Extras
  // TransformStream object.
  static void InitInternal(ScriptState*,
                           v8::Local<v8::Object> stream,
                           Member<ReadableStream>*,
                           Member<WritableStream>*,
                           ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_WRAPPER_H_
