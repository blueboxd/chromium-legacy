// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_track_writer_parameters.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_track_reader.h"
#include "third_party/blink/renderer/modules/webcodecs/video_track_writer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

class VideoTrackReaderWriterTest : public testing::Test {
 public:
  void TearDown() override {
    RunIOUntilIdle();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

 protected:
  VideoFrame* CreateBlackVideoFrame() {
    return MakeGarbageCollected<VideoFrame>(
        media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100)));
  }

  void RunIOUntilIdle() const {
    // Tracks use the IO thread to send frames to sinks. Make sure that
    // tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    platform_->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(VideoTrackReaderWriterTest, WriteAndRead) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(false);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);
  auto* reader = VideoTrackReader::Create(script_state, writer->track(),
                                          ASSERT_NO_EXCEPTION);

  auto* frame = CreateBlackVideoFrame();
  writer->writable()
      ->getWriter(script_state, ASSERT_NO_EXCEPTION)
      ->write(script_state,
              ScriptValue(scope.GetIsolate(), ToV8(frame, script_state)),
              ASSERT_NO_EXCEPTION);

  auto read_promise = reader->readable()
                          ->getReader(script_state, ASSERT_NO_EXCEPTION)
                          ->read(script_state, ASSERT_NO_EXCEPTION);
  auto v8_read_promise = read_promise.V8Value().As<v8::Promise>();

  EXPECT_EQ(v8::Promise::kPending, v8_read_promise->State());

  RunIOUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled, v8_read_promise->State());

  auto* read_frame = V8VideoFrame::ToImplWithTypeCheck(
      scope.GetIsolate(),
      v8_read_promise->Result()
          ->ToObject(scope.GetContext())
          .ToLocalChecked()
          ->Get(scope.GetContext(), V8String(scope.GetIsolate(), "value"))
          .ToLocalChecked());

  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->frame(), read_frame->frame());

  // Auto-release turned off
  EXPECT_NE(nullptr, frame->frame());
}

TEST_F(VideoTrackReaderWriterTest, AutoRelease) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(true);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);

  auto* frame = CreateBlackVideoFrame();
  writer->writable()
      ->getWriter(script_state, ASSERT_NO_EXCEPTION)
      ->write(script_state,
              ScriptValue(scope.GetIsolate(), ToV8(frame, script_state)),
              ASSERT_NO_EXCEPTION);

  RunIOUntilIdle();

  // Auto-release worked
  EXPECT_EQ(nullptr, frame->frame());
}

TEST_F(VideoTrackReaderWriterTest, Abort) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(false);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(writer->track()->readyState(), "live");

  writer->writable()->abort(script_state, ASSERT_NO_EXCEPTION);

  RunIOUntilIdle();

  EXPECT_TRUE(writer->track()->Ended());
}

}  // namespace blink
