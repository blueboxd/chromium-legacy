// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_native.h"
#include "third_party/blink/renderer/core/streams/readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "v8/include/v8.h"

namespace blink {

void V8ReadableStream::ConstructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_ReadableStream_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "ReadableStream");
  ScriptState* script_state =
      ScriptState::From(info.NewTarget().As<v8::Object>()->CreationContext());

  ScriptValue underlying_source =
      ScriptValue(info.GetIsolate(), v8::Undefined(info.GetIsolate()));
  ScriptValue strategy =
      ScriptValue(info.GetIsolate(), v8::Undefined(info.GetIsolate()));
  int num_args = info.Length();
  if (num_args >= 1) {
    underlying_source = ScriptValue(info.GetIsolate(), info[0]);
  }
  if (num_args >= 2)
    strategy = ScriptValue(info.GetIsolate(), info[1]);

  v8::Local<v8::Object> wrapper = info.Holder();
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    auto* impl = MakeGarbageCollected<ReadableStreamNative>(
        script_state, underlying_source, strategy, false, exception_state);
    if (exception_state.HadException()) {
      return;
    }
    wrapper = impl->AssociateWithWrapper(
        info.GetIsolate(), V8ReadableStream::GetWrapperTypeInfo(), wrapper);
  } else {
    auto* impl = MakeGarbageCollected<ReadableStreamWrapper>();
    wrapper = impl->AssociateWithWrapper(
        info.GetIsolate(), V8ReadableStream::GetWrapperTypeInfo(), wrapper);

    impl->Init(script_state, underlying_source, strategy, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }
  V8SetReturnValue(info, wrapper);
}

}  // namespace blink
