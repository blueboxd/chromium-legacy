// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for transferable streams. See design doc
// https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit

#include "third_party/blink/renderer/core/streams/transferable_streams.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "v8/include/v8.h"

// See the design doc at
// https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit
// for explanation of how transferable streams are constructed from the "cross
// realm identity transform" implemented in this file.

// The peer (the other end of the MessagePort) is untrusted as it may be
// compromised. This means we have to be very careful in unpacking the messages
// from the peer. LOG(WARNING) is used for cases where a message from the peer
// appears to be invalid. If this appears during ordinary testing it indicates a
// bug.
//
// The -vmodule=transferable_streams=3 command-line argument can be used for
// debugging of the protocol.

namespace blink {

namespace {

// These are the types of messages that are sent between peers.
enum class MessageType { kPull, kCancel, kChunk, kClose, kAbort, kError };

// Creates a JavaScript object with a null prototype structured like {key1:
// value2, key2: value2}. This is used to create objects to be serialized by
// postMessage.
v8::Local<v8::Object> CreateKeyValueObject(v8::Isolate* isolate,
                                           const char* key1,
                                           v8::Local<v8::Value> value1,
                                           const char* key2,
                                           v8::Local<v8::Value> value2) {
  v8::Local<v8::Name> names[] = {V8AtomicString(isolate, key1),
                                 V8AtomicString(isolate, key2)};
  v8::Local<v8::Value> values[] = {value1, value2};
  static_assert(base::size(names) == base::size(values),
                "names and values arrays must be the same size");
  return v8::Object::New(isolate, v8::Null(isolate), names, values,
                         base::size(names));
}

// Unpacks an object created by CreateKeyValueObject(). |value1| and |value2|
// are out parameters. Returns false on failure.
bool UnpackKeyValueObject(ScriptState* script_state,
                          v8::Local<v8::Object> object,
                          const char* key1,
                          v8::Local<v8::Value>* value1,
                          const char* key2,
                          v8::Local<v8::Value>* value2) {
  auto* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto context = script_state->GetContext();
  if (!object->Get(context, V8AtomicString(isolate, key1)).ToLocal(value1)) {
    DLOG(WARNING) << "Error reading key: '" << key1 << "'";
    return false;
  }
  if (!object->Get(context, V8AtomicString(isolate, key2)).ToLocal(value2)) {
    DLOG(WARNING) << "Error reading key: '" << key2 << "'";
    return false;
  }
  return true;
}

// Sends a message with type |type| and contents |value| over |port|. The type
// is packed as a number with key "t", and the value is packed with key "v".
void PackAndPostMessage(ScriptState* script_state,
                        MessagePort* port,
                        MessageType type,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  DVLOG(3) << "PackAndPostMessage sending message type "
           << static_cast<int>(type);
  auto* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> packed = CreateKeyValueObject(
      isolate, "t", v8::Number::New(isolate, static_cast<int>(type)), "v",
      value);
  port->postMessage(script_state, ScriptValue(isolate, packed),
                    PostMessageOptions::Create(), exception_state);
}

// Sends a kError message to the remote side, disregarding failure.
void SendError(ScriptState* script_state,
               MessagePort* port,
               v8::Local<v8::Value> error) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kUnknownContext, "", "");
  PackAndPostMessage(script_state, port, MessageType::kError, error,
                     exception_state);
  if (exception_state.HadException()) {
    DLOG(WARNING) << "Disregarding exception while sending error";
    exception_state.ClearException();
  }
}

// Same as PackAndPostMessage(), except that it attempts to handle exceptions by
// sending a kError message to the remote side. On failure |error| is set to the
// original exception and the function returns false. Any error from sending the
// kError message is ignored.
bool PackAndPostMessageHandlingExceptions(ScriptState* script_state,
                                          MessagePort* port,
                                          MessageType type,
                                          v8::Local<v8::Value> value,
                                          v8::Local<v8::Value>* error) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kUnknownContext, "", "");
  PackAndPostMessage(script_state, port, type, value, exception_state);

  if (exception_state.HadException()) {
    *error = exception_state.GetException();
    SendError(script_state, port, *error);
    exception_state.ClearException();
    return false;
  }

  return true;
}

// Base class for CrossRealmTransformWritable and CrossRealmTransformReadable.
// Contains common methods that are used when handling MessagePort events.
class CrossRealmTransformStream
    : public GarbageCollected<CrossRealmTransformStream> {
 public:
  // Neither of the subclasses require finalization, so no destructor.

  virtual ScriptState* GetScriptState() const = 0;
  virtual MessagePort* GetMessagePort() const = 0;

  // HandleMessage() is called by CrossRealmTransformMessageListener to handle
  // an incoming message from the MessagePort.
  virtual void HandleMessage(MessageType type, v8::Local<v8::Value> value) = 0;

  // HandleError() is called by CrossRealmTransformErrorListener when an error
  // event is fired on the message port. It should error the stream.
  virtual void HandleError(v8::Local<v8::Value> error) = 0;

  virtual void Trace(Visitor*) const {}
};

// Handles MessageEvents from the MessagePort.
class CrossRealmTransformMessageListener final : public NativeEventListener {
 public:
  explicit CrossRealmTransformMessageListener(CrossRealmTransformStream* target)
      : target_(target) {}

  void Invoke(ExecutionContext*, Event* event) override {
    // TODO(ricea): Find a way to guarantee this cast is safe.
    MessageEvent* message = static_cast<MessageEvent*>(event);
    ScriptState* script_state = target_->GetScriptState();
    // The deserializer code called by message->data() looks up the ScriptState
    // from the current context, so we need to make sure it is set.
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> data = message->data(script_state).V8Value();
    if (!data->IsObject()) {
      DLOG(WARNING) << "Invalid message from peer ignored (not object)";
      return;
    }

    v8::Local<v8::Value> type;
    v8::Local<v8::Value> value;
    if (!UnpackKeyValueObject(script_state, data.As<v8::Object>(), "t", &type,
                              "v", &value)) {
      DLOG(WARNING) << "Invalid message from peer ignored";
      return;
    }

    if (!type->IsNumber()) {
      DLOG(WARNING) << "Invalid message from peer ignored (type is not number)";
      return;
    }

    int type_value = type.As<v8::Number>()->Value();
    DVLOG(3) << "MessageListener saw message type " << type_value;
    target_->HandleMessage(static_cast<MessageType>(type_value), value);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    NativeEventListener::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformStream> target_;
};

// Handles "error" events from the MessagePort.
class CrossRealmTransformErrorListener final : public NativeEventListener {
 public:
  explicit CrossRealmTransformErrorListener(CrossRealmTransformStream* target)
      : target_(target) {}

  void Invoke(ExecutionContext*, Event*) override {
    ScriptState* script_state = target_->GetScriptState();
    const auto* error =
        DOMException::Create("chunk could not be cloned", "DataCloneError");
    auto* message_port = target_->GetMessagePort();
    v8::Local<v8::Value> error_value = ToV8(error, script_state);

    SendError(script_state, message_port, error_value);

    message_port->close();
    target_->HandleError(error_value);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    NativeEventListener::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformStream> target_;
};

// Class for data associated with the writable side of the cross realm transform
// stream.
class CrossRealmTransformWritable final : public CrossRealmTransformStream {
 public:
  CrossRealmTransformWritable(ScriptState* script_state, MessagePort* port)
      : script_state_(script_state),
        message_port_(port),
        backpressure_promise_(
            MakeGarbageCollected<StreamPromiseResolver>(script_state)) {}

  WritableStream* CreateWritableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_; }
  MessagePort* GetMessagePort() const override { return message_port_; }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(message_port_);
    visitor->Trace(backpressure_promise_);
    visitor->Trace(controller_);
    CrossRealmTransformStream::Trace(visitor);
  }

 private:
  class WriteAlgorithm;
  class CloseAlgorithm;
  class AbortAlgorithm;

  const Member<ScriptState> script_state_;
  const Member<MessagePort> message_port_;
  Member<StreamPromiseResolver> backpressure_promise_;
  Member<WritableStreamDefaultController> controller_;
};

class CrossRealmTransformWritable::WriteAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit WriteAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends the chunk to the readable side, possibly after waiting for
  // backpressure.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    auto chunk = argv[0];

    if (!writable_->backpressure_promise_) {
      return DoWrite(script_state, chunk);
    }

    auto* isolate = script_state->GetIsolate();
    return StreamThenPromise(
        script_state->GetContext(),
        writable_->backpressure_promise_->V8Promise(isolate),
        MakeGarbageCollected<DoWriteOnResolve>(script_state, chunk, this));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  // A promise handler which calls DoWrite() when the promise resolves.
  class DoWriteOnResolve final : public PromiseHandlerWithValue {
   public:
    DoWriteOnResolve(ScriptState* script_state,
                     v8::Local<v8::Value> chunk,
                     WriteAlgorithm* target)
        : PromiseHandlerWithValue(script_state),
          chunk_(script_state->GetIsolate(), chunk),
          target_(target) {}

    v8::Local<v8::Value> CallWithLocal(v8::Local<v8::Value>) override {
      ScriptState* script_state = GetScriptState();
      return target_->DoWrite(script_state,
                              chunk_.NewLocal(script_state->GetIsolate()));
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(chunk_);
      visitor->Trace(target_);
      PromiseHandlerWithValue::Trace(visitor);
    }

   private:
    const TraceWrapperV8Reference<v8::Value> chunk_;
    const Member<WriteAlgorithm> target_;
  };

  // Sends a chunk over the message port to the readable side.
  v8::Local<v8::Promise> DoWrite(ScriptState* script_state,
                                 v8::Local<v8::Value> chunk) {
    writable_->backpressure_promise_ =
        MakeGarbageCollected<StreamPromiseResolver>(script_state);

    v8::Local<v8::Value> error;
    bool success = PackAndPostMessageHandlingExceptions(
        script_state, writable_->message_port_, MessageType::kChunk, chunk,
        &error);
    if (!success) {
      writable_->message_port_->close();
      return PromiseReject(script_state, error);
    }

    return PromiseResolveWithUndefined(script_state);
  }

  const Member<CrossRealmTransformWritable> writable_;
};

class CrossRealmTransformWritable::CloseAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit CloseAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends a close message to the readable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);

    v8::Local<v8::Value> error;
    bool success = PackAndPostMessageHandlingExceptions(
        script_state, writable_->message_port_, MessageType::kClose,
        v8::Undefined(script_state->GetIsolate()), &error);

    writable_->message_port_->close();

    if (!success) {
      return PromiseReject(script_state, error);
    }

    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformWritable> writable_;
};

class CrossRealmTransformWritable::AbortAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit AbortAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends an abort message to the readable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];

    v8::Local<v8::Value> error;
    bool success = PackAndPostMessageHandlingExceptions(
        script_state, writable_->message_port_, MessageType::kAbort, reason,
        &error);

    writable_->message_port_->close();

    if (!success) {
      return PromiseReject(script_state, error);
    }

    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformWritable> writable_;
};

WritableStream* CrossRealmTransformWritable::CreateWritableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateWritableStream() can only be called once";

  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  auto* stream =
      WritableStream::Create(script_state_, CreateTrivialStartAlgorithm(),
                             MakeGarbageCollected<WriteAlgorithm>(this),
                             MakeGarbageCollected<CloseAlgorithm>(this),
                             MakeGarbageCollected<AbortAlgorithm>(this), 1,
                             CreateDefaultSizeAlgorithm(), exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  controller_ = stream->Controller();
  return stream;
}

void CrossRealmTransformWritable::HandleMessage(MessageType type,
                                                v8::Local<v8::Value> value) {
  switch (type) {
    case MessageType::kPull:
      DCHECK(backpressure_promise_);
      backpressure_promise_->ResolveWithUndefined(script_state_);
      backpressure_promise_ = nullptr;
      return;

    case MessageType::kCancel:
    case MessageType::kError: {
      WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                     value);
      if (backpressure_promise_) {
        backpressure_promise_->ResolveWithUndefined(script_state_);
        backpressure_promise_ = nullptr;
      }
      return;
    }

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformWritable::HandleError(v8::Local<v8::Value> error) {
  WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                 error);
}

// Class for data associated with the readable side of the cross realm transform
// stream.
class CrossRealmTransformReadable final : public CrossRealmTransformStream {
 public:
  CrossRealmTransformReadable(ScriptState* script_state, MessagePort* port)
      : script_state_(script_state), message_port_(port) {}

  ReadableStream* CreateReadableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_; }
  MessagePort* GetMessagePort() const override { return message_port_; }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(message_port_);
    visitor->Trace(controller_);
    CrossRealmTransformStream::Trace(visitor);
  }

 private:
  class PullAlgorithm;
  class CancelAlgorithm;

  const Member<ScriptState> script_state_;
  const Member<MessagePort> message_port_;
  Member<ReadableStreamDefaultController> controller_;
  bool finished_ = false;
};

class CrossRealmTransformReadable::PullAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit PullAlgorithm(CrossRealmTransformReadable* readable)
      : readable_(readable) {}

  // Sends a pull message to the writable side and then waits for backpressure
  // to clear.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);
    auto* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> error;
    bool success = PackAndPostMessageHandlingExceptions(
        script_state, readable_->message_port_, MessageType::kPull,
        v8::Undefined(isolate), &error);

    if (!success) {
      readable_->message_port_->close();
      return PromiseReject(script_state, error);
    }

    // The Streams Standard guarantees that PullAlgorithm won't be called again
    // until Enqueue() is called.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformReadable> readable_;
};

class CrossRealmTransformReadable::CancelAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit CancelAlgorithm(CrossRealmTransformReadable* readable)
      : readable_(readable) {}

  // Sends a cancel message to the writable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];
    readable_->finished_ = true;

    v8::Local<v8::Value> error;
    bool success = PackAndPostMessageHandlingExceptions(
        script_state, readable_->message_port_, MessageType::kCancel, reason,
        &error);

    readable_->message_port_->close();

    if (!success) {
      return PromiseReject(script_state, error);
    }

    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformReadable> readable_;
};

ReadableStream* CrossRealmTransformReadable::CreateReadableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateReadableStream can only be called once";

  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  auto* stream = ReadableStream::Create(
      script_state_, CreateTrivialStartAlgorithm(),
      MakeGarbageCollected<PullAlgorithm>(this),
      MakeGarbageCollected<CancelAlgorithm>(this),
      /* highWaterMark = */ 0, CreateDefaultSizeAlgorithm(), exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  controller_ = stream->GetController();
  return stream;
}

void CrossRealmTransformReadable::HandleMessage(MessageType type,
                                                v8::Local<v8::Value> value) {
  switch (type) {
    case MessageType::kChunk: {
      if (ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
        // This can't throw because we always use the default strategy size
        // algorithm, which doesn't throw, and always returns a valid value of
        // 1.0.
        ReadableStreamDefaultController::Enqueue(script_state_, controller_,
                                                 value, ASSERT_NO_EXCEPTION);
      }
      return;
    }

    case MessageType::kClose:
      finished_ = true;
      if (ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
        ReadableStreamDefaultController::Close(script_state_, controller_);
      }
      message_port_->close();
      return;

    case MessageType::kAbort:
    case MessageType::kError: {
      finished_ = true;
      ReadableStreamDefaultController::Error(script_state_, controller_, value);
      message_port_->close();
      return;
    }

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformReadable::HandleError(v8::Local<v8::Value> error) {
  ReadableStreamDefaultController::Error(script_state_, controller_, error);
}

}  // namespace

CORE_EXPORT WritableStream* CreateCrossRealmTransformWritable(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<CrossRealmTransformWritable>(script_state, port)
      ->CreateWritableStream(exception_state);
}

CORE_EXPORT ReadableStream* CreateCrossRealmTransformReadable(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<CrossRealmTransformReadable>(script_state, port)
      ->CreateReadableStream(exception_state);
}

}  // namespace blink
