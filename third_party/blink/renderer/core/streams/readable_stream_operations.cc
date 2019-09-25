// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

base::Optional<bool> BooleanOperationWithRethrow(
    ScriptState* script_state,
    ScriptValue value,
    const char* operation,
    ExceptionState& exception_state) {
  DCHECK(!value.IsEmpty());

  if (!value.IsObject())
    return false;

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {value.V8Value()};
  v8::Local<v8::Value> local_value;

  if (!V8ScriptRunner::CallExtra(script_state, operation, args)
           .ToLocal(&local_value)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return base::nullopt;
  }

  DCHECK(!block.HasCaught());
  return local_value->BooleanValue(script_state->GetIsolate());
}

// Performs |operation| on |value|, catching any exceptions. This is for use in
// DCHECK(). It is unsafe for general use because it ignores errors. Returns
// |fallback_value|, which must be chosen so that the DCHECK() passed if an
// exception was thrown, so that the behaviour matches a release build.
bool BooleanOperationForDCheck(ScriptState* script_state,
                               ScriptValue value,
                               const char* operation,
                               bool fallback_value) {
  v8::Local<v8::Value> args[] = {value.V8Value()};
  v8::Local<v8::Value> result_value;
  v8::TryCatch block(script_state->GetIsolate());
  if (V8ScriptRunner::CallExtra(script_state, operation, args)
          .ToLocal(&result_value)) {
    DCHECK(!block.HasCaught());
    return result_value->BooleanValue(script_state->GetIsolate());
  }
  DCHECK(block.HasCaught() ||
         script_state->GetIsolate()->IsExecutionTerminating());
  return fallback_value;
}

// Performs IsReadableStreamDefaultReader(value), catching exceptions. Should
// only be used in DCHECK(). Returns true on exception.
bool IsDefaultReaderForDCheck(ScriptState* script_state, ScriptValue value) {
  return BooleanOperationForDCheck(script_state, value,
                                   "IsReadableStreamDefaultReader", true);
}

}  // namespace

ScriptValue ReadableStreamOperations::CreateReadableStream(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    ScriptValue strategy) {
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Value> js_underlying_source =
      ToV8(underlying_source, script_state);
  v8::Local<v8::Value> js_strategy = strategy.V8Value();
  v8::Local<v8::Value> args[] = {js_underlying_source, js_strategy};
  return ScriptValue(
      script_state->GetIsolate(),
      V8ScriptRunner::CallExtra(
          script_state, "createReadableStreamWithExternalController", args));
}

ScriptValue ReadableStreamOperations::CreateReadableStream(
    ScriptState* script_state,
    ScriptValue underlying_source,
    ScriptValue strategy,
    ExceptionState& exception_state) {
  ScriptState::Scope scope(script_state);

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {underlying_source.V8Value(),
                                 strategy.V8Value()};
  v8::Local<v8::Value> result;

  if (!V8ScriptRunner::CallExtra(script_state, "createReadableStream", args)
           .ToLocal(&result)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

ScriptValue ReadableStreamOperations::CreateCountQueuingStrategy(
    ScriptState* script_state,
    size_t high_water_mark) {
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Value> args[] = {
      v8::Number::New(script_state->GetIsolate(), high_water_mark)};
  return ScriptValue(
      script_state->GetIsolate(),
      V8ScriptRunner::CallExtra(script_state,
                                "createBuiltInCountQueuingStrategy", args));
}

ScriptValue ReadableStreamOperations::GetReader(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {stream.V8Value()};
  ScriptValue result(
      script_state->GetIsolate(),
      V8ScriptRunner::CallExtra(script_state,
                                "AcquireReadableStreamDefaultReader", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  DCHECK(!result.IsEmpty() ||
         script_state->GetIsolate()->IsExecutionTerminating());
  return result;
}

base::Optional<bool> ReadableStreamOperations::IsReadableStream(
    ScriptState* script_state,
    ScriptValue value,
    ExceptionState& exception_state) {
  return BooleanOperationWithRethrow(script_state, value, "IsReadableStream",
                                     exception_state);
}

bool ReadableStreamOperations::IsReadableStreamForDCheck(
    ScriptState* script_state,
    ScriptValue value) {
  return BooleanOperationForDCheck(script_state, value, "IsReadableStream",
                                   true);
}

base::Optional<bool> ReadableStreamOperations::IsDisturbed(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamDisturbed", exception_state);
}

bool ReadableStreamOperations::IsDisturbedForDCheck(ScriptState* script_state,
                                                    ScriptValue stream) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationForDCheck(script_state, stream,
                                   "IsReadableStreamDisturbed", false);
}

base::Optional<bool> ReadableStreamOperations::IsLocked(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(script_state, stream,
                                     "IsReadableStreamLocked", exception_state);
}

bool ReadableStreamOperations::IsLockedForDCheck(ScriptState* script_state,
                                                 ScriptValue stream) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationForDCheck(script_state, stream,
                                   "IsReadableStreamLocked", false);
}

base::Optional<bool> ReadableStreamOperations::IsReadable(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamReadable", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsClosed(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(script_state, stream,
                                     "IsReadableStreamClosed", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsErrored(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamErrored", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsReadableStreamDefaultReader(
    ScriptState* script_state,
    ScriptValue value,
    ExceptionState& exception_state) {
  return BooleanOperationWithRethrow(
      script_state, value, "IsReadableStreamDefaultReader", exception_state);
}

ScriptPromise ReadableStreamOperations::DefaultReaderRead(
    ScriptState* script_state,
    ScriptValue reader) {
  DCHECK(IsDefaultReaderForDCheck(script_state, reader));

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {reader.V8Value()};
  v8::MaybeLocal<v8::Value> maybe_result = V8ScriptRunner::CallExtra(
      script_state, "ReadableStreamDefaultReaderRead", args);
  if (maybe_result.IsEmpty()) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    return ScriptPromise::Reject(script_state, block.Exception());
  }
  return ScriptPromise::Cast(script_state, maybe_result.ToLocalChecked());
}

ScriptValue ReadableStreamOperations::Tee(ScriptState* script_state,
                                          ScriptValue stream,
                                          ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  DCHECK(!IsLockedForDCheck(script_state, stream));

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {stream.V8Value()};
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallExtra(script_state, "ReadableStreamTee", args)
           .ToLocal(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

void ReadableStreamOperations::Serialize(ScriptState* script_state,
                                         ScriptValue stream,
                                         MessagePort* port,
                                         ExceptionState& exception_state) {
  DCHECK(port);
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  DCHECK(RuntimeEnabledFeatures::TransferableStreamsEnabled());
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> port_v8_value = ToV8(port, script_state);
  DCHECK(!port_v8_value.IsEmpty());
  v8::Local<v8::Value> args[] = {stream.V8Value(), port_v8_value};
  ScriptValue result(
      script_state->GetIsolate(),
      V8ScriptRunner::CallExtra(script_state, "ReadableStreamSerialize", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  return;
}

ScriptValue ReadableStreamOperations::Deserialize(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  DCHECK(port);
  DCHECK(RuntimeEnabledFeatures::TransferableStreamsEnabled());
  auto* isolate = script_state->GetIsolate();
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> port_v8 = ToV8(port, script_state);
  DCHECK(!port_v8.IsEmpty());
  v8::Local<v8::Value> args[] = {port_v8};
  ScriptValue result(script_state->GetIsolate(),
                     V8ScriptRunner::CallExtra(
                         script_state, "ReadableStreamDeserialize", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (result.IsEmpty()) {
    DCHECK(isolate->IsExecutionTerminating());
    return ScriptValue();
  }
  DCHECK(IsReadableStreamForDCheck(script_state, result));
  return result;
}

ScriptPromise ReadableStreamOperations::Cancel(
    ScriptState* script_state,
    ScriptValue stream,
    ScriptValue reason,
    ExceptionState& exception_state) {
  v8::Local<v8::Value> args[] = {stream.V8Value(), reason.V8Value()};
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> result;

  if (!V8ScriptRunner::CallExtra(script_state, "ReadableStreamCancel", args)
           .ToLocal(&result)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptPromise();
  }
  return ScriptPromise(script_state, result);
}

ScriptPromise ReadableStreamOperations::PipeTo(
    ScriptState* script_state,
    ScriptValue stream,
    ScriptValue destination,
    ScriptValue options,
    ExceptionState& exception_state) {
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> result;
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Boolean> prevent_close;
  v8::Local<v8::Boolean> prevent_abort;
  v8::Local<v8::Boolean> prevent_cancel;

  if (options.IsUndefined()) {
    // All values default to false.
    prevent_close = v8::Boolean::New(isolate, false);
    prevent_abort = v8::Boolean::New(isolate, false);
    prevent_cancel = v8::Boolean::New(isolate, false);
  } else {
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Object> v8_options;
    if (!options.V8Value()->ToObject(context).ToLocal(&v8_options)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptPromise();
    }
    v8::Local<v8::Value> prevent_close_v, prevent_abort_v, prevent_cancel_v;
    if (!v8_options->Get(context, V8String(isolate, "preventClose"))
             .ToLocal(&prevent_close_v)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptPromise();
    }
    if (!v8_options->Get(context, V8String(isolate, "preventAbort"))
             .ToLocal(&prevent_abort_v)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptPromise();
    }
    if (!v8_options->Get(context, V8String(isolate, "preventCancel"))
             .ToLocal(&prevent_cancel_v)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptPromise();
    }
    prevent_close = prevent_close_v->ToBoolean(isolate);
    prevent_abort = prevent_abort_v->ToBoolean(isolate);
    prevent_cancel = prevent_cancel_v->ToBoolean(isolate);
  }

  v8::Local<v8::Value> args[] = {stream.V8Value(), destination.V8Value(),
                                 prevent_close, prevent_abort, prevent_cancel};
  if (!V8ScriptRunner::CallExtra(script_state, "ReadableStreamPipeTo", args)
           .ToLocal(&result)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptPromise();
  }

  return ScriptPromise(script_state, result);
}

}  // namespace blink
