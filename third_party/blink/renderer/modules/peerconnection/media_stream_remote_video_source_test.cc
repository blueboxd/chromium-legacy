// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_remote_video_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/webrtc/track_observer.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/rtp_packet_infos.h"
#include "third_party/webrtc/api/video/color_space.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/system_wrappers/include/clock.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {
// On Linux the clock in WebRTC and Chromium are more or less the same.
// On Windows they are not the same and the accuracy of the measured time
// difference is typically in the range [-1, 1] ms. However, for certain builds
// such as ASAN it can be in the order of 10-20 ms. Since this is compensated
// for both here and in MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
// we need to use the worst case difference between these two measurements.
float kChromiumWebRtcMaxTimeDiffMs = 40.0f;

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::Gt;
using ::testing::SaveArg;
using ::testing::Sequence;
}  // namespace

webrtc::VideoFrame::Builder CreateBlackFrameBuilder() {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(8, 8);
  webrtc::I420Buffer::SetBlack(buffer);
  return webrtc::VideoFrame::Builder().set_video_frame_buffer(buffer);
}

class MediaStreamRemoteVideoSourceUnderTest
    : public blink::MediaStreamRemoteVideoSource {
 public:
  explicit MediaStreamRemoteVideoSourceUnderTest(
      std::unique_ptr<blink::TrackObserver> observer)
      : MediaStreamRemoteVideoSource(std::move(observer)) {}
  using MediaStreamRemoteVideoSource::EncodedSinkInterfaceForTesting;
  using MediaStreamRemoteVideoSource::SinkInterfaceForTesting;
  using MediaStreamRemoteVideoSource::StartSourceImpl;
};

class MediaStreamRemoteVideoSourceTest : public ::testing::Test {
 public:
  MediaStreamRemoteVideoSourceTest()
      : mock_factory_(new blink::MockPeerConnectionDependencyFactory()),
        webrtc_video_source_(blink::MockWebRtcVideoTrackSource::Create(
            /*supports_encoded_output=*/true)),
        webrtc_video_track_(
            blink::MockWebRtcVideoTrack::Create("test", webrtc_video_source_)),
        remote_source_(nullptr),
        number_of_successful_track_starts_(0),
        number_of_failed_track_starts_(0),
        time_diff_(base::TimeTicks::Now() - base::TimeTicks() -
                   base::TimeDelta::FromMicroseconds(rtc::TimeMicros())) {}

  void SetUp() override {
    scoped_refptr<base::SingleThreadTaskRunner> main_thread =
        blink::scheduler::GetSingleThreadTaskRunnerForTesting();

    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    std::unique_ptr<blink::TrackObserver> track_observer;
    mock_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            [](scoped_refptr<base::SingleThreadTaskRunner> main_thread,
               webrtc::MediaStreamTrackInterface* webrtc_track,
               std::unique_ptr<blink::TrackObserver>* track_observer,
               base::WaitableEvent* waitable_event) {
              track_observer->reset(
                  new blink::TrackObserver(main_thread, webrtc_track));
              waitable_event->Signal();
            },
            main_thread, CrossThreadUnretained(webrtc_video_track_.get()),
            CrossThreadUnretained(&track_observer),
            CrossThreadUnretained(&waitable_event))));
    waitable_event.Wait();

    remote_source_ =
        new MediaStreamRemoteVideoSourceUnderTest(std::move(track_observer));
    web_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                           blink::WebMediaStreamSource::kTypeVideo,
                           blink::WebString::FromASCII("dummy_source_name"),
                           true /* remote */);
    web_source_.SetPlatformSource(base::WrapUnique(remote_source_));
  }

  void TearDown() override {
    remote_source_->OnSourceTerminated();
    web_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamRemoteVideoSourceUnderTest* source() { return remote_source_; }

  blink::MediaStreamVideoTrack* CreateTrack() {
    bool enabled = true;
    return new blink::MediaStreamVideoTrack(
        source(),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            &MediaStreamRemoteVideoSourceTest::OnTrackStarted,
            CrossThreadUnretained(this))),
        enabled);
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_track_starts_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_track_starts_;
  }

  void StopWebRtcTrack() {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    mock_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            [](blink::MockWebRtcVideoTrack* video_track,
               base::WaitableEvent* waitable_event) {
              video_track->SetEnded();
              waitable_event->Signal();
            },
            CrossThreadUnretained(static_cast<blink::MockWebRtcVideoTrack*>(
                webrtc_video_track_.get())),
            CrossThreadUnretained(&waitable_event))));
    waitable_event.Wait();
  }

  const blink::WebMediaStreamSource& webkit_source() const {
    return web_source_;
  }

  const base::TimeDelta& time_diff() const { return time_diff_; }

 private:
  void OnTrackStarted(blink::WebPlatformMediaStreamSource* source,
                      blink::mojom::MediaStreamRequestResult result,
                      const blink::WebString& result_name) {
    ASSERT_EQ(source, remote_source_);
    if (result == blink::mojom::MediaStreamRequestResult::OK)
      ++number_of_successful_track_starts_;
    else
      ++number_of_failed_track_starts_;
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  std::unique_ptr<blink::MockPeerConnectionDependencyFactory> mock_factory_;
  scoped_refptr<webrtc::VideoTrackSourceInterface> webrtc_video_source_;
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_video_track_;
  // |remote_source_| is owned by |web_source_|.
  MediaStreamRemoteVideoSourceUnderTest* remote_source_;
  blink::WebMediaStreamSource web_source_;
  int number_of_successful_track_starts_;
  int number_of_failed_track_starts_;
  // WebRTC Chromium timestamp diff
  const base::TimeDelta time_diff_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, StartTrack) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  webrtc::I420Buffer::SetBlack(buffer);

  source()->SinkInterfaceForTesting()->OnFrame(
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_rotation(webrtc::kVideoRotation_0)
          .set_timestamp_us(1000)
          .build());
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationWithEncodedSinkAdded) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  source()->OnSourceTerminated();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationBeforeEncodedSinkAdded) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  source()->OnSourceTerminated();
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationBeforeRequestRefreshFrame) {
  source()->OnSourceTerminated();
  source()->RequestRefreshFrame();
}

TEST_F(MediaStreamRemoteVideoSourceTest, SurvivesSourceTermination) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink.state());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            webkit_source().GetReadyState());
  StopWebRtcTrack();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            webkit_source().GetReadyState());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink.state());

  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, PreservesColorSpace) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));
  webrtc::ColorSpace kColorSpace(webrtc::ColorSpace::PrimaryID::kSMPTE240M,
                                 webrtc::ColorSpace::TransferID::kSMPTE240M,
                                 webrtc::ColorSpace::MatrixID::kSMPTE240M,
                                 webrtc::ColorSpace::RangeID::kLimited);
  const webrtc::VideoFrame& input_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_timestamp_ms(0)
          .set_rotation(webrtc::kVideoRotation_0)
          .set_color_space(kColorSpace)
          .build();
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);
  EXPECT_TRUE(output_frame->ColorSpace() ==
              gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE240M,
                              gfx::ColorSpace::TransferID::SMPTE240M,
                              gfx::ColorSpace::MatrixID::SMPTE240M,
                              gfx::ColorSpace::RangeID::LIMITED));
  track->RemoveSink(&sink);
}

// These tests depend on measuring the difference between the internal WebRTC
// clock and Chromium's clock. Due to this they are performance sensitive and
// appear to be flaky for builds with ASAN enabled.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PopulateRequestAnimationFrameMetadata \
  DISABLED_PopulateRequestAnimationFrameMetadata
#define MAYBE_ReferenceTimeEqualsTimestampUs \
  DISABLED_ReferenceTimeEqualsTimestampUs
#else
#define MAYBE_PopulateRequestAnimationFrameMetadata \
  PopulateRequestAnimationFrameMetadata
#define MAYBE_ReferenceTimeEqualsTimestampUs ReferenceTimeEqualsTimestampUs
#endif
TEST_F(MediaStreamRemoteVideoSourceTest,
       MAYBE_PopulateRequestAnimationFrameMetadata) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  uint32_t kSsrc = 0;
  const std::vector<uint32_t> kCsrcs;
  uint32_t kRtpTimestamp = 123456;
  float kProcessingTime = 0.014;

  const webrtc::Timestamp kProcessingFinish =
      webrtc::Timestamp::Millis(rtc::TimeMillis());
  const webrtc::Timestamp kProcessingStart =
      kProcessingFinish - webrtc::TimeDelta::Millis(1.0e3 * kProcessingTime);
  const webrtc::Timestamp kCaptureTime =
      kProcessingStart - webrtc::TimeDelta::Millis(20.0);
  webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
  const int64_t ntp_offset =
      clock->CurrentNtpInMilliseconds() - clock->TimeInMilliseconds();
  const webrtc::Timestamp kCaptureTimeNtp =
      kCaptureTime + webrtc::TimeDelta::Millis(ntp_offset);
  // Expected capture time in Chromium epoch.
  base::TimeTicks kExpectedCaptureTime =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(kCaptureTime.ms()) +
      time_diff();

  webrtc::RtpPacketInfos::vector_type packet_infos;
  for (int i = 0; i < 4; ++i) {
    packet_infos.emplace_back(kSsrc, kCsrcs, kRtpTimestamp, absl::nullopt,
                              absl::nullopt, kProcessingStart.ms() - 100 + i);
  }
  // Expected receive time should be the same as the last arrival time, in
  // Chromium epoch.
  base::TimeTicks kExpectedReceiveTime =
      base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(kProcessingStart.ms() - 100 + 3) +
      time_diff();

  webrtc::VideoFrame input_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_timestamp_rtp(kRtpTimestamp)
          .set_ntp_time_ms(kCaptureTimeNtp.ms())
          .set_packet_infos(webrtc::RtpPacketInfos(packet_infos))
          .build();

  input_frame.set_processing_time({kProcessingStart, kProcessingFinish});
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  base::TimeDelta elapsed;
  EXPECT_TRUE(output_frame->metadata()->GetTimeDelta(
      media::VideoFrameMetadata::PROCESSING_TIME, &elapsed));
  EXPECT_FLOAT_EQ(elapsed.InSecondsF(), kProcessingTime);

  base::TimeTicks capture_time;
  EXPECT_TRUE(output_frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &capture_time));
  EXPECT_NEAR((capture_time - kExpectedCaptureTime).InMillisecondsF(), 0.0f,
              kChromiumWebRtcMaxTimeDiffMs);

  base::TimeTicks receive_time;
  EXPECT_TRUE(output_frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::RECEIVE_TIME, &receive_time));
  EXPECT_NEAR((receive_time - kExpectedReceiveTime).InMillisecondsF(), 0.0f,
              kChromiumWebRtcMaxTimeDiffMs);

  double rtp_timestamp;
  EXPECT_TRUE(output_frame->metadata()->GetDouble(
      media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp));
  EXPECT_EQ(static_cast<uint32_t>(rtp_timestamp), kRtpTimestamp);

  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, MAYBE_ReferenceTimeEqualsTimestampUs) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  int64_t kTimestampUs = rtc::TimeMicros();
  webrtc::VideoFrame input_frame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_us(kTimestampUs)
                                       .build();

  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  base::TimeTicks reference_time;
  EXPECT_TRUE(output_frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time));
  EXPECT_NEAR((reference_time -
               (base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(kTimestampUs) + time_diff()))
                  .InMillisecondsF(),
              0.0f, kChromiumWebRtcMaxTimeDiffMs);
  track->RemoveSink(&sink);
}

// This is a special case that is used to signal "render immediately".
TEST_F(MediaStreamRemoteVideoSourceTest, NoTimestampUsMeansNoReferenceTime) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  webrtc::VideoFrame input_frame =
      webrtc::VideoFrame::Builder().set_video_frame_buffer(buffer).build();

  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  base::TimeTicks reference_time;
  EXPECT_FALSE(output_frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time));

  track->RemoveSink(&sink);
}

class TestEncodedVideoFrame : public webrtc::RecordableEncodedFrame {
 public:
  explicit TestEncodedVideoFrame(webrtc::Timestamp timestamp)
      : timestamp_(timestamp) {}

  rtc::scoped_refptr<const webrtc::EncodedImageBufferInterface> encoded_buffer()
      const override {
    return nullptr;
  }
  absl::optional<webrtc::ColorSpace> color_space() const override {
    return absl::nullopt;
  }
  webrtc::VideoCodecType codec() const override {
    return webrtc::kVideoCodecVP8;
  }
  bool is_key_frame() const override { return true; }
  EncodedResolution resolution() const override {
    return EncodedResolution{0, 0};
  }
  webrtc::Timestamp render_time() const override { return timestamp_; }

 private:
  webrtc::Timestamp timestamp_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, ForwardsEncodedVideoFrames) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsFramesWithIncreasingTimestampsWithNullSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), /*is_link_secure=*/false);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(0).build());
  // Spin until the time counter changes.
  base::TimeTicks now = base::TimeTicks::Now();
  while (base::TimeTicks::Now() == now) {
  }
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(0).build());
  run_loop.Run();
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsFramesWithIncreasingTimestampsWithSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), /*is_link_secure=*/false);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(4711).build());
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(4712).build());
  run_loop.Run();
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsEncodedFramesWithIncreasingTimestampsWithNullSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnEncodedVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  // Spin until the time counter changes.
  base::TimeTicks now = base::TimeTicks::Now();
  while (base::TimeTicks::Now() == now) {
  }
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsEncodedFramesWithIncreasingTimestampsWithSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnEncodedVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(42)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(43)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

}  // namespace blink
