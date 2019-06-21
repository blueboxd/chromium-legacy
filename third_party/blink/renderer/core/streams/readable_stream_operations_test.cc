// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_extras_test_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/readable_stream_wrapper.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class ReadableStreamOperationsTestNotReached : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    ReadableStreamOperationsTestNotReached* self =
        MakeGarbageCollected<ReadableStreamOperationsTestNotReached>(
            script_state);
    return self->BindToV8Function();
  }

  explicit ReadableStreamOperationsTestNotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  ScriptValue Call(ScriptValue) override;
};

ScriptValue ReadableStreamOperationsTestNotReached::Call(ScriptValue) {
  EXPECT_TRUE(false) << "'Unreachable' code was reached";
  return ScriptValue();
}

class Iteration final : public GarbageCollectedFinalized<Iteration> {
 public:
  Iteration() : is_set_(false), is_done_(false), is_valid_(true) {}

  void Set(ScriptValue v) {
    DCHECK(!v.IsEmpty());
    is_set_ = true;
    v8::TryCatch block(v.GetScriptState()->GetIsolate());
    v8::Local<v8::Value> value;
    v8::Local<v8::Value> item = v.V8Value();
    if (!item->IsObject() ||
        !V8UnpackIteratorResult(v.GetScriptState(), item.As<v8::Object>(),
                                &is_done_)
             .ToLocal(&value)) {
      is_valid_ = false;
      return;
    }
    value_ = ToCoreString(
        value->ToString(v.GetScriptState()->GetContext()).ToLocalChecked());
  }

  bool IsSet() const { return is_set_; }
  bool IsDone() const { return is_done_; }
  bool IsValid() const { return is_valid_; }
  const String& Value() const { return value_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  bool is_set_;
  bool is_done_;
  bool is_valid_;
  String value_;
};

class ReaderFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                Iteration* iteration) {
    ReaderFunction* self =
        MakeGarbageCollected<ReaderFunction>(script_state, iteration);
    return self->BindToV8Function();
  }

  ReaderFunction(ScriptState* script_state, Iteration* iteration)
      : ScriptFunction(script_state), iteration_(iteration) {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(iteration_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    iteration_->Set(value);
    return value;
  }

  Member<Iteration> iteration_;
};

// Returns the internal V8 Extras implementation of a ReadableStream object.
// Requires StreamsNative feature to be off.
ScriptValue CheckedGetInternalStream(ScriptState* script_state,
                                     ReadableStream* readable_stream) {
  CHECK(!RuntimeEnabledFeatures::StreamsNativeEnabled());
  ReadableStreamWrapper* readable_stream_wrapper =
      static_cast<ReadableStreamWrapper*>(readable_stream);
  return readable_stream_wrapper->GetInternalStream(script_state);
}

ScriptValue CheckedGetInternalStream(ScriptState* script_state,
                                     ScriptValue stream) {
  ReadableStream* readable_stream =
      V8ReadableStream::ToImpl(stream.V8Value().As<v8::Object>());
  return CheckedGetInternalStream(script_state, readable_stream);
}

TEST(ReadableStreamOperationsTest, IsReadableStream) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
                   scope.GetScriptState(),
                   ScriptValue(scope.GetScriptState(),
                               v8::Undefined(scope.GetIsolate())),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
                   scope.GetScriptState(),
                   ScriptValue::CreateNull(scope.GetScriptState()),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
                   scope.GetScriptState(),
                   ScriptValue(scope.GetScriptState(),
                               v8::Object::New(scope.GetIsolate())),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  ScriptValue stream = EvalWithPrintingError(&scope, "new ReadableStream()");
  EXPECT_FALSE(stream.IsEmpty());
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
                   scope.GetScriptState(), stream, ASSERT_NO_EXCEPTION)
                   .value_or(false));
  ASSERT_TRUE(V8ReadableStream::HasInstance(stream.V8Value().As<v8::Object>(),
                                            scope.GetIsolate()));

  ScriptValue internal_stream =
      CheckedGetInternalStream(scope.GetScriptState(), stream);
  ASSERT_FALSE(internal_stream.IsEmpty());
  EXPECT_TRUE(ReadableStreamOperations::IsReadableStream(
                  scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION)
                  .value_or(false));
}

TEST(ReadableStreamOperationsTest, IsReadableStreamDefaultReaderInvalid) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
                   scope.GetScriptState(),
                   ScriptValue(scope.GetScriptState(),
                               v8::Undefined(scope.GetIsolate())),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
                   scope.GetScriptState(),
                   ScriptValue::CreateNull(scope.GetScriptState()),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
                   scope.GetScriptState(),
                   ScriptValue(scope.GetScriptState(),
                               v8::Object::New(scope.GetIsolate())),
                   ASSERT_NO_EXCEPTION)
                   .value_or(true));
  ScriptValue stream = EvalWithPrintingError(&scope, "new ReadableStream()");
  ASSERT_FALSE(stream.IsEmpty());

  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
                   scope.GetScriptState(), stream, ASSERT_NO_EXCEPTION)
                   .value_or(true));
}

TEST(ReadableStreamOperationsTest, GetReader) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  ScriptValue internal_stream =
      CheckedGetInternalStream(scope.GetScriptState(), stream);
  ASSERT_FALSE(internal_stream.IsEmpty());

  EXPECT_EQ(ReadableStreamOperations::IsLocked(
                scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  ScriptValue reader;
  reader = ReadableStreamOperations::GetReader(
      scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(reader.IsEmpty());
  EXPECT_EQ(ReadableStreamOperations::IsLocked(
                scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION),
            base::make_optional(true));

  EXPECT_EQ(ReadableStreamOperations::IsReadableStream(
                scope.GetScriptState(), reader, ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  EXPECT_EQ(ReadableStreamOperations::IsReadableStreamDefaultReader(
                scope.GetScriptState(), reader, ASSERT_NO_EXCEPTION),
            base::make_optional(true));

  // Already locked!
  DummyExceptionStateForTesting exception_state;
  reader = ReadableStreamOperations::GetReader(
      scope.GetScriptState(), internal_stream, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_TRUE(reader.IsEmpty());
}

TEST(ReadableStreamOperationsTest, IsDisturbed) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  ScriptValue internal_stream =
      CheckedGetInternalStream(scope.GetScriptState(), stream);
  EXPECT_EQ(ReadableStreamOperations::IsDisturbed(
                scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION),
            base::make_optional(false));

  stream->cancel(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  EXPECT_EQ(ReadableStreamOperations::IsDisturbed(
                scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION),
            base::make_optional(true));
}

TEST(ReadableStreamOperationsTest, Read) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue reader =
      EvalWithPrintingError(&scope,
                            "var controller;"
                            "function start(c) { controller = c; }"
                            "new ReadableStream({start}).getReader()");
  EXPECT_FALSE(reader.IsEmpty());
  ASSERT_TRUE(ReadableStreamOperations::IsReadableStreamDefaultReader(
                  scope.GetScriptState(), reader, ASSERT_NO_EXCEPTION)
                  .value_or(false));

  Iteration* it1 = MakeGarbageCollected<Iteration>();
  Iteration* it2 = MakeGarbageCollected<Iteration>();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(it1->IsSet());
  EXPECT_FALSE(it2->IsSet());

  ASSERT_FALSE(
      EvalWithPrintingError(&scope, "controller.enqueue('hello')").IsEmpty());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_FALSE(it2->IsSet());

  ASSERT_FALSE(EvalWithPrintingError(&scope, "controller.close()").IsEmpty());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_TRUE(it2->IsDone());
}

TEST(ReadableStreamOperationsTest,
     CreateReadableStreamWithCustomUnderlyingSourceAndStrategy) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());

  ScriptValue strategy = ReadableStreamOperations::CreateCountQueuingStrategy(
      scope.GetScriptState(), 10);
  ASSERT_FALSE(strategy.IsEmpty());

  ScriptValue stream = ReadableStreamOperations::CreateReadableStream(
      scope.GetScriptState(), underlying_source, strategy);
  ASSERT_FALSE(stream.IsEmpty());

  EXPECT_EQ(10, underlying_source->DesiredSize());

  underlying_source->Enqueue(ScriptValue::From(scope.GetScriptState(), "a"));
  EXPECT_EQ(9, underlying_source->DesiredSize());

  underlying_source->Enqueue(ScriptValue::From(scope.GetScriptState(), "b"));
  EXPECT_EQ(8, underlying_source->DesiredSize());

  ScriptValue reader;
  reader = ReadableStreamOperations::GetReader(scope.GetScriptState(), stream,
                                               ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(reader.IsEmpty());

  Iteration* it1 = MakeGarbageCollected<Iteration>();
  Iteration* it2 = MakeGarbageCollected<Iteration>();
  Iteration* it3 = MakeGarbageCollected<Iteration>();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it3),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_EQ(10, underlying_source->DesiredSize());

  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("a", it1->Value());

  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_FALSE(it2->IsDone());
  EXPECT_EQ("b", it2->Value());

  EXPECT_FALSE(it3->IsSet());

  underlying_source->Close();
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(it3->IsSet());
  EXPECT_TRUE(it3->IsValid());
  EXPECT_TRUE(it3->IsDone());
}

TEST(ReadableStreamOperationsTest, IsReadable) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());

  auto* readable =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(readable);

  auto* closing_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* closed = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), closing_source, 0);
  ASSERT_TRUE(closed);
  closing_source->Close();

  auto* erroring_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* errored = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), erroring_source, 0);
  ASSERT_TRUE(errored);
  erroring_source->Error(
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate())));

  EXPECT_EQ(ReadableStreamOperations::IsReadable(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), readable),
                ASSERT_NO_EXCEPTION),
            base::make_optional(true));
  EXPECT_EQ(ReadableStreamOperations::IsReadable(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), closed),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  EXPECT_EQ(ReadableStreamOperations::IsReadable(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), errored),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
}

TEST(ReadableStreamOperationsTest, IsClosed) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());

  ReadableStream* readable =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(readable);

  auto* closing_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* closed = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), closing_source, 0);
  ASSERT_TRUE(closed);
  closing_source->Close();

  auto* erroring_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* errored = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), erroring_source, 0);
  ASSERT_TRUE(errored);
  erroring_source->Error(
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate())));

  EXPECT_EQ(ReadableStreamOperations::IsClosed(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), readable),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  EXPECT_EQ(ReadableStreamOperations::IsClosed(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), closed),
                ASSERT_NO_EXCEPTION),
            base::make_optional(true));
  EXPECT_EQ(ReadableStreamOperations::IsClosed(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), errored),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
}

TEST(ReadableStreamOperationsTest, IsErrored) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());

  auto* readable =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(readable);

  auto* closing_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  ReadableStream* closed = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), closing_source, 0);
  ASSERT_TRUE(closed);
  closing_source->Close();

  auto* erroring_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* errored = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), erroring_source, 0);
  ASSERT_TRUE(errored);
  erroring_source->Error(
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate())));

  EXPECT_EQ(ReadableStreamOperations::IsErrored(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), readable),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  EXPECT_EQ(ReadableStreamOperations::IsErrored(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), closed),
                ASSERT_NO_EXCEPTION),
            base::make_optional(false));
  EXPECT_EQ(ReadableStreamOperations::IsErrored(
                scope.GetScriptState(),
                CheckedGetInternalStream(scope.GetScriptState(), errored),
                ASSERT_NO_EXCEPTION),
            base::make_optional(true));
}

TEST(ReadableStreamOperationsTest, Tee) {
  ScopedStreamsNativeForTest enabled(false);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  v8::Local<v8::Context> context = scope.GetScriptState()->GetContext();
  NonThrowableExceptionState exception_state;
  auto* source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), source, 0);
  ASSERT_TRUE(stream);

  ScriptValue result = ReadableStreamOperations::Tee(
      scope.GetScriptState(),
      CheckedGetInternalStream(scope.GetScriptState(), stream),
      exception_state);
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result.IsObject());

  v8::Local<v8::Value> v8_branch1, v8_branch2;
  ASSERT_TRUE(
      result.V8Value().As<v8::Object>()->Get(context, 0).ToLocal(&v8_branch1));
  ASSERT_TRUE(
      result.V8Value().As<v8::Object>()->Get(context, 1).ToLocal(&v8_branch2));

  ScriptValue new1(scope.GetScriptState(), v8_branch1);
  ScriptValue new2(scope.GetScriptState(), v8_branch2);

  ASSERT_TRUE(ReadableStreamOperations::IsReadableStream(
                  scope.GetScriptState(), new1, ASSERT_NO_EXCEPTION)
                  .value_or(true));
  ASSERT_TRUE(ReadableStreamOperations::IsReadableStream(
                  scope.GetScriptState(), new2, ASSERT_NO_EXCEPTION)
                  .value_or(true));

  ScriptValue reader1 = ReadableStreamOperations::GetReader(
      scope.GetScriptState(), new1, exception_state);
  ScriptValue reader2 = ReadableStreamOperations::GetReader(
      scope.GetScriptState(), new2, exception_state);

  ASSERT_FALSE(reader1.IsEmpty());
  ASSERT_FALSE(reader2.IsEmpty());

  Iteration* it1 = MakeGarbageCollected<Iteration>();
  Iteration* it2 = MakeGarbageCollected<Iteration>();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader1)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader2)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(it1->IsSet());
  EXPECT_FALSE(it2->IsSet());

  source->Enqueue(ScriptValue(scope.GetScriptState(),
                              V8String(scope.GetIsolate(), "hello")));
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_FALSE(it2->IsDone());
  EXPECT_EQ("hello", it2->Value());
}

TEST(ReadableStreamOperationsTest, Serialize) {
  ScopedStreamsNativeForTest streams_native_enabled(false);
  ScopedTransferableStreamsForTest transferable_streams_enabled(true);

  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto* source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), source, 0);
  ASSERT_TRUE(stream);

  source->Enqueue(ScriptValue(scope.GetScriptState(),
                              V8String(scope.GetIsolate(), "hello")));
  ScriptValue internal_stream =
      CheckedGetInternalStream(scope.GetScriptState(), stream);
  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());
  ReadableStreamOperations::Serialize(scope.GetScriptState(), internal_stream,
                                      channel->port1(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(ReadableStreamOperations::IsLocked(
      scope.GetScriptState(), internal_stream, ASSERT_NO_EXCEPTION));
  ScriptValue transferred = ReadableStreamOperations::Deserialize(
      scope.GetScriptState(), channel->port2(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(transferred.IsEmpty());
  ScriptValue reader = ReadableStreamOperations::GetReader(
      scope.GetScriptState(), transferred, ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(reader.IsEmpty());
  Iteration* it = MakeGarbageCollected<Iteration>();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it),
            ReadableStreamOperationsTestNotReached::CreateFunction(
                scope.GetScriptState()));
  // Let the message pass through the MessagePort.
  test::RunPendingTasks();
  // Let the Read promise resolve.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(it->IsSet());
  EXPECT_TRUE(it->IsValid());
  EXPECT_FALSE(it->IsDone());
  EXPECT_EQ("hello", it->Value());
}

}  // namespace

}  // namespace blink
