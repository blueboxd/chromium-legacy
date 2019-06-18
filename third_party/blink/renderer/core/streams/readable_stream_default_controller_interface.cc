// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

class ReadableStreamDefaultControllerWrapper final
    : public ReadableStreamDefaultControllerInterface {
 public:
  explicit ReadableStreamDefaultControllerWrapper(ScriptValue controller)
      : ReadableStreamDefaultControllerInterface(controller.GetScriptState()),
        js_controller_(controller.GetIsolate(), controller.V8Value()) {
    js_controller_.SetPhantom();
  }

  // Users of the ReadableStreamDefaultControllerWrapper can call this to note
  // that the stream has been canceled and thus they don't anticipate using the
  // ReadableStreamDefaultControllerWrapper anymore.
  // (close/desiredSize/enqueue/error will become no-ops afterward.)
  void NoteHasBeenCanceled() override { js_controller_.Clear(); }

  void Close() override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerClose", args);
    js_controller_.Clear();
    result.ToLocalChecked();
  }

  double DesiredSize() const override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return 0;

    v8::Local<v8::Value> args[] = {controller};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerGetDesiredSize", args);

    return result.ToLocalChecked().As<v8::Number>()->Value();
  }

  void Enqueue(v8::Local<v8::Value> js_chunk) const override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller, js_chunk};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerEnqueue", args);
    result.ToLocalChecked();
  }

  void Error(v8::Local<v8::Value> js_error) override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller, js_error};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerError", args);
    js_controller_.Clear();
    result.ToLocalChecked();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(script_state_);
    ReadableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  ScopedPersistent<v8::Value> js_controller_;
};

class ReadableStreamDefaultControllerNative final
    : public ReadableStreamDefaultControllerInterface {
 public:
  explicit ReadableStreamDefaultControllerNative(ScriptValue controller)
      : ReadableStreamDefaultControllerInterface(controller.GetScriptState()) {
    DCHECK(RuntimeEnabledFeatures::StreamsNativeEnabled());

    v8::Local<v8::Object> controller_object =
        controller.V8Value().As<v8::Object>();
    controller_ = V8ReadableStreamDefaultController::ToImpl(controller_object);

    DCHECK(controller_);
  }

  void NoteHasBeenCanceled() override { controller_ = nullptr; }

  void Close() override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ReadableStreamDefaultController::Close(script_state_, controller_);
    controller_ = nullptr;
  }

  double DesiredSize() const override {
    if (!controller_)
      return 0.0;

    base::Optional<double> desired_size = controller_->GetDesiredSize();
    DCHECK(desired_size.has_value());
    return desired_size.value();
  }

  void Enqueue(v8::Local<v8::Value> js_chunk) const override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ExceptionState exception_state(script_state_->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");
    ReadableStreamDefaultController::Enqueue(script_state_, controller_,
                                             js_chunk, exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from Enqueue()";
      exception_state.ClearException();
    }
  }

  void Error(v8::Local<v8::Value> js_error) override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ReadableStreamDefaultController::Error(script_state_, controller_,
                                           js_error);
    controller_ = nullptr;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    ReadableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  Member<ReadableStreamDefaultController> controller_;
};

}  // namespace

ReadableStreamDefaultControllerInterface*
ReadableStreamDefaultControllerInterface::Create(ScriptValue controller) {
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    return MakeGarbageCollected<ReadableStreamDefaultControllerNative>(
        controller);
  }

  return MakeGarbageCollected<ReadableStreamDefaultControllerWrapper>(
      controller);
}

ReadableStreamDefaultControllerInterface::
    ReadableStreamDefaultControllerInterface(ScriptState* script_state)
    : script_state_(script_state) {}

ReadableStreamDefaultControllerInterface::
    ~ReadableStreamDefaultControllerInterface() = default;

void ReadableStreamDefaultControllerInterface::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
}

}  // namespace blink
