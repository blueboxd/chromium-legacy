// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_extras_test_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
using ::testing::_;
using ::testing::Mock;

class TransformStreamTest : public ::testing::Test {
 public:
  TransformStreamTest() {}

  TransformStream* Stream() const { return stream_; }

  void Init(TransformStreamTransformer* transformer,
            ScriptState* script_state,
            ExceptionState& exception_state) {
    stream_ = MakeGarbageCollected<TransformStream>();
    stream_->Init(transformer, script_state, exception_state);
  }

  // This takes the |readable| and |writable| properties of the TransformStream
  // and copies them onto the global object so they can be accessed by Eval().
  void CopyReadableAndWritableToGlobal(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ReadableStream* readable = Stream()->Readable();
    WritableStream* writable = Stream()->Writable();
    v8::Local<v8::Object> global = script_state->GetContext()->Global();
    EXPECT_TRUE(global
                    ->Set(scope.GetContext(),
                          V8String(scope.GetIsolate(), "readable"),
                          ToV8(readable, script_state))
                    .IsJust());
    EXPECT_TRUE(global
                    ->Set(scope.GetContext(),
                          V8String(scope.GetIsolate(), "writable"),
                          ToV8(writable, script_state))
                    .IsJust());
  }

 private:
  Persistent<TransformStream> stream_;
};

class IdentityTransformer final : public TransformStreamTransformer {
 public:
  explicit IdentityTransformer(ScriptState* script_state)
      : script_state_(script_state) {}

  void Transform(v8::Local<v8::Value> chunk,
                 TransformStreamDefaultControllerInterface* controller,
                 ExceptionState& exception_state) override {
    controller->Enqueue(chunk, exception_state);
  }

  void Flush(TransformStreamDefaultControllerInterface* controller,
             ExceptionState& exception_state) override {}

  ScriptState* GetScriptState() override { return script_state_; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

class MockTransformStreamTransformer : public TransformStreamTransformer {
 public:
  explicit MockTransformStreamTransformer(ScriptState* script_state)
      : script_state_(script_state) {}

  MOCK_METHOD3(Transform,
               void(v8::Local<v8::Value> chunk,
                    TransformStreamDefaultControllerInterface*,
                    ExceptionState&));
  MOCK_METHOD2(Flush,
               void(TransformStreamDefaultControllerInterface*,
                    ExceptionState&));

  ScriptState* GetScriptState() override { return script_state_; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

// If this doesn't work then nothing else will.
TEST_F(TransformStreamTest, Construct) {
  V8TestingScope scope;
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(Stream());
}

TEST_F(TransformStreamTest, Accessors) {
  V8TestingScope scope;
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ReadableStream* readable = Stream()->Readable();
  WritableStream* writable = Stream()->Writable();
  EXPECT_TRUE(readable);
  EXPECT_TRUE(writable);
}

TEST_F(TransformStreamTest, TransformIsCalled) {
  V8TestingScope scope;
  auto* mock = MakeGarbageCollected<MockTransformStreamTransformer>(
      scope.GetScriptState());
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Transform(_, _, _));

  // The initial read is needed to relieve backpressure.
  EvalWithPrintingError(&scope,
                        "readable.getReader().read();\n"
                        "const writer = writable.getWriter();\n"
                        "writer.write('a');\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

TEST_F(TransformStreamTest, FlushIsCalled) {
  V8TestingScope scope;
  auto* mock = MakeGarbageCollected<MockTransformStreamTransformer>(
      scope.GetScriptState());
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Flush(_, _));

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

bool IsIteratorForStringMatching(ScriptState* script_state,
                                 ScriptValue value,
                                 const String& expected) {
  if (!value.IsObject()) {
    return false;
  }
  bool done = false;
  auto chunk = V8UnpackIteratorResult(
      script_state,
      value.V8Value()->ToObject(script_state->GetContext()).ToLocalChecked(),
      &done);
  if (done || chunk.IsEmpty())
    return false;
  return ToCoreStringWithUndefinedOrNullCheck(chunk.ToLocalChecked()) ==
         expected;
}

bool IsTypeError(ScriptState* script_state,
                 ScriptValue value,
                 const String& message) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()->ToObject(script_state->GetContext()).ToLocal(&object)) {
    return false;
  }
  if (!object->IsNativeError())
    return false;

  const auto& Has = [script_state, object](const String& key,
                                           const String& value) -> bool {
    v8::Local<v8::Value> actual;
    return object
               ->Get(script_state->GetContext(),
                     V8AtomicString(script_state->GetIsolate(), key))
               .ToLocal(&actual) &&
           ToCoreStringWithUndefinedOrNullCheck(actual) == value;
  };

  return Has("name", "TypeError") && Has("message", message);
}

TEST_F(TransformStreamTest, EnqueueFromTransform) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.write('a');\n");

  ReadableStream* readable = Stream()->Readable();
  auto* read_handle =
      readable->GetReadHandle(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_handle->Read(script_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(IsIteratorForStringMatching(script_state, tester.Value(), "a"));
}

TEST_F(TransformStreamTest, EnqueueFromFlush) {
  class EnqueueFromFlushTransformer : public TransformStreamTransformer {
   public:
    explicit EnqueueFromFlushTransformer(ScriptState* script_state)
        : script_state_(script_state) {}

    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultControllerInterface*,
                   ExceptionState&) override {}
    void Flush(TransformStreamDefaultControllerInterface* controller,
               ExceptionState& exception_state) override {
      controller->Enqueue(ToV8("a", script_state_), exception_state);
    }
    ScriptState* GetScriptState() override { return script_state_; }
    void Trace(Visitor* visitor) override {
      visitor->Trace(script_state_);
      TransformStreamTransformer::Trace(visitor);
    }

   private:
    const Member<ScriptState> script_state_;
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<EnqueueFromFlushTransformer>(script_state),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  ReadableStream* readable = Stream()->Readable();
  auto* read_handle =
      readable->GetReadHandle(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_handle->Read(script_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(IsIteratorForStringMatching(script_state, tester.Value(), "a"));
}

TEST_F(TransformStreamTest, ThrowFromTransform) {
  static constexpr char kMessage[] = "errorInTransform";
  class ThrowFromTransformTransformer : public TransformStreamTransformer {
   public:
    explicit ThrowFromTransformTransformer(ScriptState* script_state)
        : script_state_(script_state) {}
    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultControllerInterface*,
                   ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
    void Flush(TransformStreamDefaultControllerInterface*,
               ExceptionState&) override {}

    ScriptState* GetScriptState() override { return script_state_; }
    void Trace(Visitor* visitor) override {
      visitor->Trace(script_state_);
      TransformStreamTransformer::Trace(visitor);
    }

   private:
    const Member<ScriptState> script_state_;
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<ThrowFromTransformTransformer>(
           scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.write('a');\n");

  ReadableStream* readable = Stream()->Readable();
  auto* read_handle =
      readable->GetReadHandle(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state,
                                  read_handle->Read(script_state));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
  ScriptPromiseTester write_tester(script_state,
                                   ScriptPromise::Cast(script_state, promise));
  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, write_tester.Value(), kMessage));
}

TEST_F(TransformStreamTest, ThrowFromFlush) {
  static constexpr char kMessage[] = "errorInFlush";
  class ThrowFromFlushTransformer : public TransformStreamTransformer {
   public:
    explicit ThrowFromFlushTransformer(ScriptState* script_state)
        : script_state_(script_state) {}
    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultControllerInterface*,
                   ExceptionState&) override {}
    void Flush(TransformStreamDefaultControllerInterface*,
               ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
    ScriptState* GetScriptState() override { return script_state_; }
    void Trace(Visitor* visitor) override {
      visitor->Trace(script_state_);
      TransformStreamTransformer::Trace(visitor);
    }

   private:
    const Member<ScriptState> script_state_;
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<ThrowFromFlushTransformer>(scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.close();\n");

  ReadableStream* readable = Stream()->Readable();
  auto* read_handle =
      readable->GetReadHandle(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state,
                                  read_handle->Read(script_state));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
  ScriptPromiseTester write_tester(script_state,
                                   ScriptPromise::Cast(script_state, promise));
  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, write_tester.Value(), kMessage));
}

TEST_F(TransformStreamTest, CreateFromReadableWritablePair) {
  V8TestingScope scope;
  ReadableStream* readable =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  WritableStream* writable =
      WritableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  TransformStream transform(readable, writable);
  EXPECT_EQ(readable, transform.Readable());
  EXPECT_EQ(writable, transform.Writable());
}

}  // namespace
}  // namespace blink
