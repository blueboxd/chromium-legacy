// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_wrapper.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/streams/writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

class ReadableStreamWrapper::ReadHandleImpl final
    : public ReadableStream::ReadHandle {
 public:
  ReadHandleImpl(v8::Isolate* isolate, v8::Local<v8::Value> reader)
      : reader_(isolate, reader) {}

  ~ReadHandleImpl() override = default;

  ScriptPromise Read(ScriptState* script_state) override {
    return ReadableStreamOperations::DefaultReaderRead(
        script_state,
        ScriptValue(script_state->GetIsolate(),
                    reader_.NewLocal(script_state->GetIsolate())));
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(reader_);
    ReadHandle::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Value> reader_;
};

void ReadableStreamWrapper::Init(ScriptState* script_state,
                                 ScriptValue underlying_source,
                                 ScriptValue strategy,
                                 ExceptionState& exception_state) {
  ScriptValue value = ReadableStreamOperations::CreateReadableStream(
      script_state, underlying_source, strategy, exception_state);
  if (exception_state.HadException())
    return;

  DCHECK(value.IsObject());
  InitWithInternalStream(script_state, value.V8Value().As<v8::Object>(),
                         exception_state);
}

void ReadableStreamWrapper::InitWithInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  DCHECK(ReadableStreamOperations::IsReadableStreamForDCheck(
      script_state, ScriptValue(script_state->GetIsolate(), object)));
  object_.Set(script_state->GetIsolate(), object);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> wrapper = ToV8(this, script_state);
  if (wrapper.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Object> bindings =
      context->GetExtrasBindingObject().As<v8::Object>();
  v8::Local<v8::Value> symbol_value;
  if (!bindings->Get(context, V8String(isolate, "internalReadableStreamSymbol"))
           .ToLocal(&symbol_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  if (wrapper.As<v8::Object>()
          ->Set(context, symbol_value.As<v8::Symbol>(), object)
          .IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
}

ReadableStreamWrapper* ReadableStreamWrapper::Create(
    ScriptState* script_state,
    ScriptValue underlying_source,
    ScriptValue strategy,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStreamWrapper>();
  stream->Init(script_state, underlying_source, strategy, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateFromInternalStream(
    ScriptState* script_state,
    ScriptValue object,
    ExceptionState& exception_state) {
  DCHECK(object.IsObject());
  return CreateFromInternalStream(
      script_state, object.V8Value().As<v8::Object>(), exception_state);
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateFromInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStreamWrapper>();
  stream->InitWithInternalStream(script_state, object, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark) {
  v8::TryCatch block(script_state->GetIsolate());
  ScriptValue strategy =
      ReadableStreamOperations::CreateCountQueuingStrategy(script_state, 0);
  if (strategy.IsEmpty())
    return nullptr;

  ScriptValue value = ReadableStreamOperations::CreateReadableStream(
      script_state, underlying_source, strategy);
  if (value.IsEmpty())
    return nullptr;

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "ReadableStream");
  DCHECK(value.V8Value()->IsObject());
  auto* stream = CreateFromInternalStream(script_state, value, exception_state);
  if (exception_state.HadException())
    exception_state.ClearException();
  return stream;
}

void ReadableStreamWrapper::Trace(Visitor* visitor) {
  visitor->Trace(object_);
  ReadableStream::Trace(visitor);
}

ScriptPromise ReadableStreamWrapper::cancel(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  return cancel(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

bool ReadableStreamWrapper::locked(ScriptState* script_state,
                                   ExceptionState& exception_state) const {
  auto result = IsLocked(script_state, exception_state);

  return !result || *result;
}

ScriptPromise ReadableStreamWrapper::cancel(ScriptState* script_state,
                                            ScriptValue reason,
                                            ExceptionState& exception_state) {
  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("Cannot cancel a locked stream");
  }

  if (exception_state.HadException())
    return ScriptPromise();

  return ReadableStreamOperations::Cancel(
      script_state, GetInternalStream(script_state), reason, exception_state);
}

ScriptValue ReadableStreamWrapper::getReader(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
}

ScriptValue ReadableStreamWrapper::getReader(ScriptState* script_state,
                                             ScriptValue options,
                                             ExceptionState& exception_state) {
  GetReaderValidateOptions(script_state, options, exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }
  return ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
}

ScriptValue ReadableStreamWrapper::pipeThrough(
    ScriptState* script_state,
    ScriptValue transform_stream,
    ExceptionState& exception_state) {
  return pipeThrough(script_state, transform_stream,
                     ScriptValue(script_state->GetIsolate(),
                                 v8::Undefined(script_state->GetIsolate())),
                     exception_state);
}

// https://streams.spec.whatwg.org/#rs-pipe-through
ScriptValue ReadableStreamWrapper::pipeThrough(
    ScriptState* script_state,
    ScriptValue transform_stream,
    ScriptValue options,
    ExceptionState& exception_state) {
  ScriptValue readable;
  WritableStream* writable = nullptr;
  PipeThroughExtractReadableWritable(script_state, this, transform_stream,
                                     &readable, &writable, exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  DCHECK(!RuntimeEnabledFeatures::StreamsNativeEnabled());

  // This cast is safe because the following code will only be run when the
  // native version of WritableStream is not in use.
  WritableStreamWrapper* writable_wrapper =
      static_cast<WritableStreamWrapper*>(writable);

  // 8. Let _promise_ be ! ReadableStreamPipeTo(*this*, _writable_,
  //    _preventClose_, _preventAbort_, _preventCancel_,
  //   _signal_).

  ScriptPromise promise = ReadableStreamOperations::PipeTo(
      script_state, GetInternalStream(script_state),
      writable_wrapper->GetInternalStream(script_state), options,
      exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  // 9. Set _promise_.[[PromiseIsHandled]] to *true*.
  promise.MarkAsHandled();

  // 10. Return _readable_.
  return readable;
}

ScriptPromise ReadableStreamWrapper::pipeTo(ScriptState* script_state,
                                            ScriptValue destination,
                                            ExceptionState& exception_state) {
  return pipeTo(script_state, destination,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ScriptPromise ReadableStreamWrapper::pipeTo(ScriptState* script_state,
                                            ScriptValue destination_value,
                                            ScriptValue options,
                                            ExceptionState& exception_state) {
  WritableStream* destination = PipeToCheckSourceAndDestination(
      script_state, this, destination_value, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }
  DCHECK(destination);

  DCHECK(!RuntimeEnabledFeatures::StreamsNativeEnabled());

  // This cast is safe because the following code will only be run when the
  // native version of WritableStream is not in use.
  WritableStreamWrapper* destination_wrapper =
      static_cast<WritableStreamWrapper*>(destination);

  return ReadableStreamOperations::PipeTo(
      script_state, GetInternalStream(script_state),
      destination_wrapper->GetInternalStream(script_state), options,
      exception_state);
}

ScriptValue ReadableStreamWrapper::tee(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return CallTeeAndReturnBranchArray(script_state, this, exception_state);
}

void ReadableStreamWrapper::Tee(ScriptState* script_state,
                                ReadableStream** branch1,
                                ReadableStream** branch2,
                                ExceptionState& exception_state) {
  v8::Local<v8::Context> context = script_state->GetContext();

  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("The stream is already locked.");
  }

  if (exception_state.HadException())
    return;

  ScriptValue tee_result = ReadableStreamOperations::Tee(
      script_state, GetInternalStream(script_state), exception_state);
  if (tee_result.IsEmpty())
    return;

  DCHECK(!exception_state.HadException());
  DCHECK(tee_result.V8Value()->IsArray());

  v8::Local<v8::Array> branches = tee_result.V8Value().As<v8::Array>();
  v8::Local<v8::Value> v8_branch1, v8_branch2;
  v8::TryCatch block(script_state->GetIsolate());

  if (!branches->Get(context, 0).ToLocal(&v8_branch1)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (!branches->Get(context, 1).ToLocal(&v8_branch2)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  DCHECK(v8_branch1->IsObject());
  DCHECK(v8_branch2->IsObject());

  ReadableStreamWrapper* temp_branch1 =
      MakeGarbageCollected<ReadableStreamWrapper>();
  ReadableStreamWrapper* temp_branch2 =
      MakeGarbageCollected<ReadableStreamWrapper>();

  temp_branch1->InitWithInternalStream(
      script_state, v8_branch1.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return;

  temp_branch2->InitWithInternalStream(
      script_state, v8_branch2.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return;

  *branch1 = temp_branch1;
  *branch2 = temp_branch2;
}

ReadableStream::ReadHandle* ReadableStreamWrapper::GetReadHandle(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return MakeGarbageCollected<ReadHandleImpl>(script_state->GetIsolate(),
                                              reader.V8Value());
}

base::Optional<bool> ReadableStreamWrapper::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsLocked(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsDisturbed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsDisturbed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsReadable(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsReadable(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsClosed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsClosed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsErrored(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsErrored(
      script_state, GetInternalStream(script_state), exception_state);
}

void ReadableStreamWrapper::LockAndDisturb(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  ScriptState::Scope scope(script_state);

  const base::Optional<bool> is_locked =
      IsLocked(script_state, exception_state);
  if (!is_locked || is_locked.value())
    return;

  ScriptValue reader = getReader(script_state, exception_state);
  if (reader.IsEmpty())
    return;

  ScriptPromise promise =
      ReadableStreamOperations::DefaultReaderRead(script_state, reader);
  promise.MarkAsHandled();
}

void ReadableStreamWrapper::Serialize(ScriptState* script_state,
                                      MessagePort* port,
                                      ExceptionState& exception_state) {
  ReadableStreamOperations::Serialize(
      script_state, GetInternalStream(script_state), port, exception_state);
}

// static
ReadableStreamWrapper* ReadableStreamWrapper::Deserialize(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  // We need to execute V8 Extras JavaScript to create the new ReadableStream.
  // We will not run author code.
  v8::Isolate::AllowJavascriptExecutionScope allow_js(
      script_state->GetIsolate());
  ScriptValue internal_stream = ReadableStreamOperations::Deserialize(
      script_state, port, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return CreateFromInternalStream(script_state, internal_stream,
                                  exception_state);
}

ScriptValue ReadableStreamWrapper::GetInternalStream(
    ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     object_.NewLocal(script_state->GetIsolate()));
}

}  // namespace blink
