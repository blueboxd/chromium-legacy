// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_raf/video_request_animation_frame_impl.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::Return;

namespace blink {

using VideoFramePresentationMetadata =
    WebMediaPlayer::VideoFramePresentationMetadata;

namespace {

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD0(RequestAnimationFrame, void());
  MOCK_METHOD0(GetVideoFramePresentationMetadata,
               std::unique_ptr<VideoFramePresentationMetadata>());
};

class MockFunction : public ScriptFunction {
 public:
  static testing::StrictMock<MockFunction>* Create(ScriptState* script_state) {
    return MakeGarbageCollected<testing::StrictMock<MockFunction>>(
        script_state);
  }

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }

  MOCK_METHOD1(Call, ScriptValue(ScriptValue));

 protected:
  explicit MockFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}
};

// Helper class to wrap a VideoFramePresentationData, which can't have a copy
// constructor, due to it having a media::VideoFrameMetadata instance.
class MetadataHelper {
 public:
  static VideoFramePresentationMetadata* GetDefaultMedatada() {
    InitializeFields();
    return &metadata_;
  }

  static std::unique_ptr<VideoFramePresentationMetadata> CopyDefaultMedatada() {
    InitializeFields();

    auto copy = std::make_unique<VideoFramePresentationMetadata>();

    copy->presented_frames = metadata_.presented_frames;
    copy->presentation_time = metadata_.presentation_time;
    copy->expected_presentation_time = metadata_.expected_presentation_time;
    copy->width = metadata_.width;
    copy->height = metadata_.height;
    copy->presentation_timestamp = metadata_.presentation_timestamp;
    copy->metadata.MergeMetadataFrom(&(metadata_.metadata));

    return copy;
  }

 private:
  static void InitializeFields() {
    if (initialized)
      return;

    base::TimeTicks now = base::TimeTicks::Now();
    metadata_.presented_frames = 42;
    metadata_.presentation_time = now + base::TimeDelta::FromMilliseconds(10);
    metadata_.expected_presentation_time =
        now + base::TimeDelta::FromMilliseconds(26);
    metadata_.width = 320;
    metadata_.height = 480;
    metadata_.presentation_timestamp = base::TimeDelta::FromSecondsD(3.14);
    metadata_.metadata.SetTimeDelta(media::VideoFrameMetadata::PROCESSING_TIME,
                                    base::TimeDelta::FromMilliseconds(60));
    metadata_.metadata.SetTimeTicks(
        media::VideoFrameMetadata::CAPTURE_BEGIN_TIME,
        now + base::TimeDelta::FromMilliseconds(5));
    metadata_.metadata.SetDouble(media::VideoFrameMetadata::RTP_TIMESTAMP,
                                 12345);

    initialized = true;
  }

  static bool initialized;
  static VideoFramePresentationMetadata metadata_;
};

bool MetadataHelper::initialized = false;
VideoFramePresentationMetadata MetadataHelper::metadata_;

// Helper class that compares the parameters used when invoking a callback, with
// the reference parameters we expect.
class VideoRafParameterVerifierCallback
    : public VideoFrameRequestCallbackCollection::VideoFrameCallback {
 public:
  explicit VideoRafParameterVerifierCallback(DocumentLoadTiming& timing)
      : timing_(timing) {}
  ~VideoRafParameterVerifierCallback() override = default;

  void Invoke(double now, const VideoFrameMetadata* metadata) override {
    was_invoked_ = true;
    now_ = now;

    auto* expected = MetadataHelper::GetDefaultMedatada();
    EXPECT_EQ(expected->presented_frames, metadata->presentedFrames());
    EXPECT_EQ(TicksToMillisecondsF(expected->presentation_time),
              metadata->presentationTime());
    EXPECT_EQ(TicksToMillisecondsF(expected->expected_presentation_time),
              metadata->expectedPresentationTime());
    EXPECT_EQ((unsigned int)expected->width, metadata->width());
    EXPECT_EQ((unsigned int)expected->height, metadata->height());
    EXPECT_EQ(expected->presentation_timestamp.InSecondsF(),
              metadata->presentationTimestamp());

    base::TimeDelta processing_time;
    EXPECT_TRUE(expected->metadata.GetTimeDelta(
        media::VideoFrameMetadata::PROCESSING_TIME, &processing_time));
    EXPECT_EQ(processing_time.InSecondsF(), metadata->elapsedProcessingTime());

    base::TimeTicks capture_time;
    EXPECT_TRUE(expected->metadata.GetTimeTicks(
        media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &capture_time));
    EXPECT_EQ(TicksToMillisecondsF(capture_time), metadata->captureTime());

    double rtp_timestamp;
    EXPECT_TRUE(expected->metadata.GetDouble(
        media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp));
    EXPECT_EQ(rtp_timestamp, metadata->rtpTimestamp());
  }

  double last_now() { return now_; }
  bool was_invoked() { return was_invoked_; }

 private:
  double TicksToMillisecondsF(base::TimeTicks ticks) {
    return timing_.MonotonicTimeToZeroBasedDocumentTime(ticks)
        .InMillisecondsF();
  }

  double now_;
  bool was_invoked_ = false;
  DocumentLoadTiming& timing_;
};

}  // namespace

class VideoRequestAnimationFrameImplTest
    : public PageTestBase,
      private ScopedVideoRequestAnimationFrameForTest {
 public:
  VideoRequestAnimationFrameImplTest()
      : ScopedVideoRequestAnimationFrameForTest(true) {}

  virtual void SetUpWebMediaPlayer() {
    auto mock_media_player = std::make_unique<MockWebMediaPlayer>();
    media_player_ = mock_media_player.get();
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                             std::move(mock_media_player)),
                         nullptr);
  }

  void SetUp() override {
    SetUpWebMediaPlayer();

    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->appendChild(video_);

    video()->SetSrc("http://example.com/foo.mp4");
    test::RunPendingTasks();
    UpdateAllLifecyclePhasesForTest();
  }

  HTMLVideoElement* video() { return video_.Get(); }

  MockWebMediaPlayer* media_player() { return media_player_; }

  VideoRequestAnimationFrameImpl& video_raf() {
    return VideoRequestAnimationFrameImpl::From(*video());
  }

  void SimulateFramePresented() { video_->OnRequestAnimationFrame(); }

  void SimulateAnimationFrame(base::TimeTicks now) {
    GetDocument().GetScriptedAnimationController().ServiceScriptedAnimations(
        now);
  }

  V8VideoFrameRequestCallback* GetCallback(MockFunction* function) {
    return V8VideoFrameRequestCallback::Create(function->Bind());
  }

  void RegisterCallbackDirectly(VideoRafParameterVerifierCallback* callback) {
    video_raf().RegisterCallbackForTest(callback);
  }

 private:
  Persistent<HTMLVideoElement> video_;

  // Owned by HTMLVideoElementFrameClient.
  MockWebMediaPlayer* media_player_;
};

class VideoRequestAnimationFrameImplNullMediaPlayerTest
    : public VideoRequestAnimationFrameImplTest {
 public:
  void SetUpWebMediaPlayer() override {
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                             std::unique_ptr<MockWebMediaPlayer>(),
                             /* allow_empty_client */ true),
                         nullptr);
  }
};

TEST_F(VideoRequestAnimationFrameImplTest, VerifyRequestAnimationFrame) {
  V8TestingScope scope;

  auto* function = MockFunction::Create(scope.GetScriptState());

  // Queuing up a video.rAF call should propagate to the WebMediaPlayer.
  EXPECT_CALL(*media_player(), RequestAnimationFrame()).Times(1);
  video_raf().requestAnimationFrame(GetCallback(function));

  testing::Mock::VerifyAndClear(media_player());

  // Callbacks should not be run immediately when a frame is presented.
  EXPECT_CALL(*function, Call(_)).Times(0);
  SimulateFramePresented();

  testing::Mock::VerifyAndClear(function);

  // Callbacks should be called during the rendering steps.
  EXPECT_CALL(*function, Call(_)).Times(1);
  EXPECT_CALL(*media_player(), GetVideoFramePresentationMetadata())
      .WillOnce(
          Return(ByMove(std::make_unique<VideoFramePresentationMetadata>())));
  SimulateAnimationFrame(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoRequestAnimationFrameImplTest,
       VerifyCancelAnimationFrame_BeforePresentedFrame) {
  V8TestingScope scope;

  auto* function = MockFunction::Create(scope.GetScriptState());

  // Queue and cancel a request before a frame is presented.
  int callback_id = video_raf().requestAnimationFrame(GetCallback(function));
  video_raf().cancelAnimationFrame(callback_id);

  EXPECT_CALL(*function, Call(_)).Times(0);
  SimulateFramePresented();
  SimulateAnimationFrame(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoRequestAnimationFrameImplTest,
       VerifyCancelAnimationFrame_AfterPresentedFrame) {
  V8TestingScope scope;

  auto* function = MockFunction::Create(scope.GetScriptState());

  // Queue a request.
  int callback_id = video_raf().requestAnimationFrame(GetCallback(function));
  SimulateFramePresented();

  // The callback should be scheduled for execution, but not yet run.
  EXPECT_CALL(*function, Call(_)).Times(0);
  video_raf().cancelAnimationFrame(callback_id);
  SimulateAnimationFrame(base::TimeTicks::Now());

  testing::Mock::VerifyAndClear(function);
}

TEST_F(VideoRequestAnimationFrameImplTest, VerifyParameters) {
  auto timing = GetDocument().Loader()->GetTiming();

  auto* callback =
      MakeGarbageCollected<VideoRafParameterVerifierCallback>(timing);

  // Register the non-V8 callback.
  RegisterCallbackDirectly(callback);

  EXPECT_CALL(*media_player(), GetVideoFramePresentationMetadata())
      .WillOnce(Return(ByMove(MetadataHelper::CopyDefaultMedatada())));

  double now_ms =
      timing.MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks::Now())
          .InMillisecondsF();

  // Run the callbacks directly, since they weren't scheduled to be run by the
  // ScriptedAnimationController.
  video_raf().ExecuteFrameCallbacks(now_ms);

  EXPECT_EQ(callback->last_now(), now_ms);
  EXPECT_TRUE(callback->was_invoked());

  testing::Mock::VerifyAndClear(media_player());
}

TEST_F(VideoRequestAnimationFrameImplNullMediaPlayerTest, VerifyNoCrash) {
  V8TestingScope scope;

  auto* function = MockFunction::Create(scope.GetScriptState());

  video_raf().requestAnimationFrame(GetCallback(function));

  SimulateFramePresented();
  SimulateAnimationFrame(base::TimeTicks::Now());
}

}  // namespace blink
