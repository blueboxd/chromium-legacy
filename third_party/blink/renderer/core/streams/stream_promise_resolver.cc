// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

StreamPromiseResolver* StreamPromiseResolver::CreateResolved(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);
  promise->Resolve(script_state, value);
  return promise;
}

StreamPromiseResolver* StreamPromiseResolver::CreateResolvedWithUndefined(
    ScriptState* script_state) {
  return CreateResolved(script_state,
                        v8::Undefined(script_state->GetIsolate()));
}

StreamPromiseResolver* StreamPromiseResolver::CreateRejected(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);
  promise->Reject(script_state, reason);
  return promise;
}

// The constructor crashes if a v8::Promise::Resolver object cannot be created.
// TODO(ricea): If necessary change this to make all the methods no-op instead.
StreamPromiseResolver::StreamPromiseResolver(ScriptState* script_state)
    : resolver_(script_state->GetIsolate(),
                v8::Promise::Resolver::New(script_state->GetContext())
                    .ToLocalChecked()) {}

void StreamPromiseResolver::Resolve(ScriptState* script_state,
                                    v8::Local<v8::Value> value) {
  DCHECK(!resolver_.IsEmpty());
  if (is_settled_) {
    return;
  }
  is_settled_ = true;
  auto result = resolver_.NewLocal(script_state->GetIsolate())
                    ->Resolve(script_state->GetContext(), value);
  if (result.IsNothing()) {
    DVLOG(3) << "Assuming JS shutdown and ignoring failed Resolve";
  }
}

void StreamPromiseResolver::ResolveWithUndefined(ScriptState* script_state) {
  Resolve(script_state, v8::Undefined(script_state->GetIsolate()));
}

void StreamPromiseResolver::Reject(ScriptState* script_state,
                                   v8::Local<v8::Value> reason) {
  DCHECK(!resolver_.IsEmpty());
  if (is_settled_) {
    return;
  }
  is_settled_ = true;
  auto result = resolver_.NewLocal(script_state->GetIsolate())
                    ->Reject(script_state->GetContext(), reason);
  if (result.IsNothing()) {
    DVLOG(3) << "Assuming JS shutdown and ignoring failed Reject";
  }
}

ScriptPromise StreamPromiseResolver::GetScriptPromise(
    ScriptState* script_state) const {
  return ScriptPromise(script_state, V8Promise(script_state->GetIsolate()));
}

v8::Local<v8::Promise> StreamPromiseResolver::V8Promise(
    v8::Isolate* isolate) const {
  DCHECK(!resolver_.IsEmpty());
  return resolver_.NewLocal(isolate)->GetPromise();
}

void StreamPromiseResolver::MarkAsHandled(v8::Isolate* isolate) {
  V8Promise(isolate)->MarkAsHandled();
}

v8::Promise::PromiseState StreamPromiseResolver::State(
    v8::Isolate* isolate) const {
  return V8Promise(isolate)->State();
}

void StreamPromiseResolver::Trace(Visitor* visitor) {
  visitor->Trace(resolver_);
}

}  // namespace blink
