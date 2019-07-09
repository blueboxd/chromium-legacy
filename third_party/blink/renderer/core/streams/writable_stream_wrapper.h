// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_WRAPPER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

class MessagePort;
class WritableStreamWrapper;

// This is an implementation of the WritableStream interface that delegates to
// the V8 Extras implementation.
class CORE_EXPORT WritableStreamWrapper final : public WritableStream {
 public:
  // Call one of Init functions before using the instance.
  WritableStreamWrapper() = default;
  ~WritableStreamWrapper() override = default;

  // If an error happens, |exception_state.HadException()| will be true, and
  // |this| will not be usable after that.
  void Init(ScriptState*,
            ScriptValue underlying_sink,
            ScriptValue strategy,
            ExceptionState& exception_state);

  static WritableStreamWrapper* CreateFromInternalStream(
      ScriptState* script_state,
      ScriptValue internal_stream,
      ExceptionState& exception_state) {
    DCHECK(internal_stream.IsObject());
    return CreateFromInternalStream(script_state,
                                    internal_stream.V8Value().As<v8::Object>(),
                                    exception_state);
  }
  static WritableStreamWrapper* CreateFromInternalStream(
      ScriptState*,
      v8::Local<v8::Object> internal_stream,
      ExceptionState&);

  static WritableStreamWrapper* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSinkBase*,
      size_t high_water_mark);

  void Trace(Visitor* visitor) override;

  // IDL defined functions
  bool locked(ScriptState*, ExceptionState&) const override;
  ScriptPromise abort(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;
  ScriptValue getWriter(ScriptState*, ExceptionState&) override;

  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override;

  // Serialize this stream to |port|. The stream will be locked by this
  // operation.
  void Serialize(ScriptState*, MessagePort* port, ExceptionState&) override;

  // Given a |port| which is entangled with a MessagePort that was previously
  // passed to Serialize(), returns a new WritableStreamWrapper which behaves
  // like it was the original.
  static WritableStreamWrapper* Deserialize(ScriptState*,
                                            MessagePort* port,
                                            ExceptionState&);

  ScriptValue GetInternalStream(ScriptState*) const;

 private:
  bool InitInternal(ScriptState*, v8::Local<v8::Object> internal_stream);

  static v8::MaybeLocal<v8::Object> CreateInternalStream(
      ScriptState* script_state,
      v8::Local<v8::Value> underlying_sink,
      v8::Local<v8::Value> strategy);

  TraceWrapperV8Reference<v8::Object> internal_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_WRAPPER_H_
