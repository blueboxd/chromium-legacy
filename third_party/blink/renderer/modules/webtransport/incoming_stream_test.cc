// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/incoming_stream.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/mock_web_transport_close_proxy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::StrictMock;

class IncomingStreamTest : public ::testing::Test {
 public:
  // The default value of |capacity| means some sensible value selected by mojo.
  void CreateDataPipe(uint32_t capacity = 0) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;

    MojoResult result = mojo::CreateDataPipe(&options, &data_pipe_producer_,
                                             &data_pipe_consumer_);
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "CreateDataPipe() returned " << result;
    }
  }

  IncomingStream* CreateIncomingStream(const V8TestingScope& scope,
                                       uint32_t capacity = 0) {
    CreateDataPipe(capacity);
    auto* script_state = scope.GetScriptState();
    DCHECK(!mock_close_proxy_);
    mock_close_proxy_ =
        MakeGarbageCollected<StrictMock<MockWebTransportCloseProxy>>();
    auto* incoming_stream = MakeGarbageCollected<IncomingStream>(
        script_state, mock_close_proxy_, std::move(data_pipe_consumer_));
    incoming_stream->Init();
    return incoming_stream;
  }

  void WriteToPipe(Vector<uint8_t> data) {
    uint32_t num_bytes = data.size();
    EXPECT_EQ(data_pipe_producer_->WriteData(data.data(), &num_bytes,
                                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
              MOJO_RESULT_OK);
    EXPECT_EQ(num_bytes, data.size());
  }

  void ClosePipe() { data_pipe_producer_.reset(); }

  // Copies the contents of a v8::Value containing a Uint8Array to a Vector.
  static Vector<uint8_t> ToVector(const V8TestingScope& scope,
                                  v8::Local<v8::Value> v8value) {
    Vector<uint8_t> ret;

    DOMUint8Array* value =
        V8Uint8Array::ToImplWithTypeCheck(scope.GetIsolate(), v8value);
    if (!value) {
      ADD_FAILURE() << "chunk is not an Uint8Array";
      return ret;
    }
    ret.Append(static_cast<uint8_t*>(value->Data()),
               static_cast<wtf_size_t>(value->byteLengthAsSizeT()));
    return ret;
  }

  struct Iterator {
    bool done = false;
    Vector<uint8_t> value;
  };

  // Performs a single read from |reader|, converting the output to the
  // Iterator type. Assumes that the readable stream is not errored.
  static Iterator Read(const V8TestingScope& scope,
                       ReadableStreamDefaultReader* reader) {
    auto* script_state = scope.GetScriptState();
    ScriptPromise read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, read_promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return IteratorFromReadResult(scope, tester.Value().V8Value());
  }

  static Iterator IteratorFromReadResult(const V8TestingScope& scope,
                                         v8::Local<v8::Value> result) {
    CHECK(result->IsObject());
    Iterator ret;
    v8::Local<v8::Value> v8value;
    if (!V8UnpackIteratorResult(scope.GetScriptState(), result.As<v8::Object>(),
                                &ret.done)
             .ToLocal(&v8value)) {
      ADD_FAILURE() << "Couldn't unpack iterator";
      return ret;
    }
    if (ret.done) {
      EXPECT_TRUE(v8value->IsUndefined());
      return ret;
    }

    ret.value = ToVector(scope, v8value);
    return ret;
  }

  Persistent<MockWebTransportCloseProxy> mock_close_proxy_;
  mojo::ScopedDataPipeProducerHandle data_pipe_producer_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer_;
};

TEST_F(IncomingStreamTest, Create) {
  V8TestingScope scope;
  auto* incoming_stream = CreateIncomingStream(scope);
  EXPECT_TRUE(incoming_stream->readable());

  EXPECT_CALL(*mock_close_proxy_, ForgetStream());
}

TEST_F(IncomingStreamTest, AbortReading) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise reading_aborted = incoming_stream->readingAborted();

  EXPECT_CALL(*mock_close_proxy_, ForgetStream());

  incoming_stream->abortReading(nullptr);

  // Allow the close signal to propagate down the pipe.
  test::RunPendingTasks();

  // Check that the pipe was closed.
  const char data[] = "foo";
  uint32_t num_bytes = 3;
  EXPECT_EQ(data_pipe_producer_->WriteData(data, &num_bytes,
                                           MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
            MOJO_RESULT_FAILED_PRECONDITION);

  ScriptPromiseTester abort_tester(script_state, reading_aborted);
  abort_tester.WaitUntilSettled();
  EXPECT_TRUE(abort_tester.IsFulfilled());

  // Calling abortReading() does not error the stream, it simply closes it.
  Iterator result = Read(scope, reader);
  EXPECT_TRUE(result.done);
}

TEST_F(IncomingStreamTest, ReadArrayBuffer) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  WriteToPipe({'A'});

  Iterator result = Read(scope, reader);
  EXPECT_FALSE(result.done);
  EXPECT_THAT(result.value, ElementsAre('A'));
  EXPECT_CALL(*mock_close_proxy_, ForgetStream());
}

// Reading data followed by a remote close should not lose data.
TEST_F(IncomingStreamTest, ReadThenClosedWithFin) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  WriteToPipe({'B'});
  incoming_stream->OnIncomingStreamClosed(true);

  Iterator result1 = Read(scope, reader);
  EXPECT_FALSE(result1.done);
  EXPECT_THAT(result1.value, ElementsAre('B'));

  // This write arrives "out of order" due to the data pipe not being
  // synchronised with the mojo interface.
  WriteToPipe({'C'});
  ClosePipe();

  Iterator result2 = Read(scope, reader);
  EXPECT_FALSE(result2.done);
  EXPECT_THAT(result2.value, ElementsAre('C'));

  Iterator result3 = Read(scope, reader);
  EXPECT_TRUE(result3.done);
}

// Reading data followed by a remote abort should not lose data.
TEST_F(IncomingStreamTest, ReadThenClosedWithoutFin) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  WriteToPipe({'B'});
  incoming_stream->OnIncomingStreamClosed(false);

  Iterator result1 = Read(scope, reader);
  EXPECT_FALSE(result1.done);
  EXPECT_THAT(result1.value, ElementsAre('B'));

  // This write arrives "out of order" due to the data pipe not being
  // synchronized with the mojo interface.
  WriteToPipe({'C'});
  ClosePipe();

  Iterator result2 = Read(scope, reader);
  EXPECT_FALSE(result2.done);

  // Even if the stream is not cleanly closed, we still endeavour to deliver all
  // data.
  EXPECT_THAT(result2.value, ElementsAre('C'));

  ScriptPromise result3 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester result3_tester(script_state, result3);
  result3_tester.WaitUntilSettled();
  EXPECT_TRUE(result3_tester.IsRejected());
  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), result3_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->code(),
            static_cast<uint16_t>(DOMExceptionCode::kNetworkError));
  EXPECT_EQ(exception->message(),
            "The stream was aborted by the remote server");
}

TEST_F(IncomingStreamTest, DataPipeResetBeforeClosedWithFin) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  WriteToPipe({'E'});
  ClosePipe();
  incoming_stream->OnIncomingStreamClosed(true);

  Iterator result1 = Read(scope, reader);
  EXPECT_FALSE(result1.done);
  EXPECT_THAT(result1.value, ElementsAre('E'));

  Iterator result2 = Read(scope, reader);
  EXPECT_TRUE(result2.done);
}

TEST_F(IncomingStreamTest, DataPipeResetBeforeClosedWithoutFin) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  WriteToPipe({'F'});
  ClosePipe();
  incoming_stream->OnIncomingStreamClosed(false);

  Iterator result1 = Read(scope, reader);
  EXPECT_FALSE(result1.done);
  EXPECT_THAT(result1.value, ElementsAre('F'));

  ScriptPromise result2 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester result2_tester(script_state, result2);
  result2_tester.WaitUntilSettled();
  EXPECT_TRUE(result2_tester.IsRejected());
  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), result2_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->code(),
            static_cast<uint16_t>(DOMExceptionCode::kNetworkError));
  EXPECT_EQ(exception->message(),
            "The stream was aborted by the remote server");
}

// A live stream will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(IncomingStreamTest, GarbageCollection) {
  V8TestingScope scope;

  WeakPersistent<IncomingStream> incoming_stream;

  {
    // The readable stream created when creating a IncomingStream creates some
    // v8 handles. To ensure these are collected, we need to create a handle
    // scope. This is not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    incoming_stream = CreateIncomingStream(scope);
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |incoming_stream| pointer as references.
  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  ASSERT_TRUE(incoming_stream);

  auto* script_state = scope.GetScriptState();

  EXPECT_CALL(*mock_close_proxy_, ForgetStream());

  ScriptPromise cancel_promise;
  {
    // Cancelling also creates v8 handles, so we need a new handle scope as
    // above.
    v8::HandleScope handle_scope(scope.GetIsolate());
    cancel_promise =
        incoming_stream->readable()->cancel(script_state, ASSERT_NO_EXCEPTION);
  }

  ScriptPromiseTester tester(script_state, cancel_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  EXPECT_FALSE(incoming_stream);
}

TEST_F(IncomingStreamTest, GarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<IncomingStream> incoming_stream;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    incoming_stream = CreateIncomingStream(scope);
  }

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  ASSERT_TRUE(incoming_stream);

  // Close the other end of the pipe.
  ClosePipe();

  test::RunPendingTasks();

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  ASSERT_TRUE(incoming_stream);

  incoming_stream->OnIncomingStreamClosed(false);

  test::RunPendingTasks();

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  EXPECT_FALSE(incoming_stream);
}

TEST_F(IncomingStreamTest, WriteToPipeWithPendingRead) {
  V8TestingScope scope;

  auto* incoming_stream = CreateIncomingStream(scope);
  auto* script_state = scope.GetScriptState();
  auto* reader =
      incoming_stream->readable()->getReader(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  test::RunPendingTasks();

  WriteToPipe({'A'});

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  Iterator result = IteratorFromReadResult(scope, tester.Value().V8Value());
  EXPECT_FALSE(result.done);
  EXPECT_THAT(result.value, ElementsAre('A'));
  EXPECT_CALL(*mock_close_proxy_, ForgetStream());
}

}  // namespace

}  // namespace blink
