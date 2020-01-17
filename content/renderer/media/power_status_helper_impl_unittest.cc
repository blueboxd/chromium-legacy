// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/power_status_helper_impl.h"

#include <tuple>

//#include "base/metrics/histogram.h"
//#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/device/public/mojom/battery_status.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::_;
using testing::AnyNumber;
using testing::Bool;
using testing::Combine;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::ResultOf;
using testing::Return;
using testing::Values;

class PowerStatusHelperImplTest : public testing::Test {
 public:
  class MockBatteryMonitor : public device::mojom::BatteryMonitor {
   public:
    MOCK_METHOD0(DidGetBatteryMonitor, void());
    MOCK_METHOD0(DidQueryNextStatus, void());
    MOCK_METHOD0(DidDisconnect, void());

    ~MockBatteryMonitor() {
      // Mojo gets mad if we don't finish up outstanding callbacks.
      if (callback_)
        ProvidePowerUpdate(0, 0);
    }

    // device::mojom::BatteryMonitor
    void QueryNextStatus(QueryNextStatusCallback callback) override {
      DidQueryNextStatus();
      callback_ = std::move(callback);
    }

    // Would be nice to use a MockCallback for this, but a move-only return type
    // doesn't seem to work.
    mojo::PendingRemote<device::mojom::BatteryMonitor> GetBatteryMonitor() {
      DidGetBatteryMonitor();
      switch (remote_type_) {
        case RemoteType::kConnected:
        case RemoteType::kDisconnected: {
          auto pending = receiver_.BindNewPipeAndPassRemote();
          receiver_.set_disconnect_handler(base::BindOnce(
              &MockBatteryMonitor::DidDisconnect, base::Unretained(this)));
          if (remote_type_ == RemoteType::kDisconnected)
            receiver_.reset();
          base::RunLoop().RunUntilIdle();
          return pending;
        }
        case RemoteType::kEmpty:
          return mojo::PendingRemote<device::mojom::BatteryMonitor>();
      }
    }

    // Would be nice if this were base::MockCallback, but move-only types don't
    // seem to work.
    PowerStatusHelperImpl::CreateBatteryMonitorCB cb() {
      return base::BindRepeating(&MockBatteryMonitor::GetBatteryMonitor,
                                 base::Unretained(this));
    }

    // Provide a battery update via |callback_|.
    void ProvidePowerUpdate(bool is_charging, float current_level) {
      EXPECT_TRUE(callback_);
      device::mojom::BatteryStatusPtr status =
          device::mojom::BatteryStatus::New(is_charging, 0, /* charging time */
                                            0, /* discharging time */
                                            current_level);
      std::move(callback_).Run(std::move(status));
      base::RunLoop().RunUntilIdle();
    }

    mojo::Receiver<device::mojom::BatteryMonitor> receiver_{this};

    // If false, then GetBatteryMonitor will not return a monitor.
    enum class RemoteType {
      // Provide a connected remote.
      kConnected,
      // Provide an empty PendingRemote
      kEmpty,
      // Provide a PendingRemote to a disconnected remote.
      kDisconnected
    };
    RemoteType remote_type_ = RemoteType::kConnected;

    // Most recently provided callback.
    QueryNextStatusCallback callback_;
  };

  void SetUp() override {
    helper_ = std::make_unique<PowerStatusHelperImpl>(monitor_.cb());
  }

  // Set up |helper_| to be in a state that should record. Returns the bucket.
  // |alternate| just causes us to create a different recordable bucket.
  int MakeRecordable(bool alternate = false) {
    helper_->SetIsPlaying(true);
    media::PipelineMetadata metadata;
    metadata.has_video = true;
    metadata.video_decoder_config = media::VideoDecoderConfig(
        media::kCodecH264, media::H264PROFILE_MAIN,
        media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        media::VideoColorSpace(), media::VideoTransformation(),
        gfx::Size(0, 0),        /* coded_size */
        gfx::Rect(0, 0),        /* visible rect */
        gfx::Size(640, 360),    /* natural size */
        std::vector<uint8_t>(), /* extra_data */
        media::EncryptionScheme::kUnencrypted);
    helper_->SetMetadata(metadata);
    helper_->SetAverageFrameRate(60);
    // Use |alternate| to set fullscreen state, since that should still be
    // recordable but in a different bucket.
    helper_->SetIsFullscreen(alternate);
    base::RunLoop().RunUntilIdle();
    helper_->UpdatePowerExperimentState(true);
    base::RunLoop().RunUntilIdle();

    return PowerStatusHelperImpl::kCodecBitsH264 |
           PowerStatusHelperImpl::kResolution360p |
           PowerStatusHelperImpl::kFrameRate60 |
           (alternate ? PowerStatusHelperImpl::kFullScreenYes
                      : PowerStatusHelperImpl::kFullScreenNo);
  }

  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  // Verify that we've added |battery_delta| and |time_delta| to |bucket| in
  // both histograms.
  void VerifyHistogramDelta(int bucket,
                            int battery_delta,
                            base::TimeDelta time_delta) {
    // Since histograms are cumulative, include the new counts.
    total_battery_delta += battery_delta;
    total_time_delta += time_delta.InMilliseconds();
    histogram_tester_.ExpectBucketCount(helper_->BatteryDeltaHistogram(),
                                        bucket, total_battery_delta);
    histogram_tester_.ExpectBucketCount(helper_->ElapsedTimeHistogram(), bucket,
                                        total_time_delta);
  }

  // Previous total histogram counts.  Note that we record the total in msec,
  // rather than as a TimeDelta, so that we round the same way as the helper.
  int total_battery_delta = 0;
  int total_time_delta = 0;  // msec

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockBatteryMonitor monitor_;

  // Helper under test
  std::unique_ptr<PowerStatusHelperImpl> helper_;

  base::HistogramTester histogram_tester_;
};

TEST_F(PowerStatusHelperImplTest, EmptyPendingRemoteIsOkay) {
  // Enable power monitoring, but have the callback fail to provide a remote.
  // This should be handled gracefully.

  // Ask |monitor_| not to provide a remote, and expect that |helper_| asks.
  monitor_.remote_type_ = MockBatteryMonitor::RemoteType::kEmpty;
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  MakeRecordable();
}

TEST_F(PowerStatusHelperImplTest, UnboundPendingRemoteIsOkay) {
  // TODO: this doesn't run the "is bound" part.  maybe we should just delete
  // the "is bound" part, or switch to a disconnection handler, etc.
  monitor_.remote_type_ = MockBatteryMonitor::RemoteType::kDisconnected;
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  MakeRecordable();
}

TEST_F(PowerStatusHelperImplTest, BasicReportingWithFractionalAmounts) {
  // Send three power updates, and verify that an update is called for the
  // last two.  The update should be fractional, so that some of it is rolled
  // over to the next call.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  const int bucket = MakeRecordable();

  const float baseline_level = 0.9;
  // Will round to 10%.
  const float second_level = baseline_level - 0.106;
  // Will round to 11% (plus a little).
  const float third_level = second_level - 0.106;

  // This should be the baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);

  // This should trigger recording.
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  FastForward(time_delta);

  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, second_level);
  VerifyHistogramDelta(bucket, 10, time_delta);

  // This should also record, and pick up the fractional percentage drop that
  // wasn't included in the previous one.
  FastForward(time_delta);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, third_level);
  VerifyHistogramDelta(bucket, 11, time_delta);
}

TEST_F(PowerStatusHelperImplTest, ChargingResetsBaseline) {
  // Send some power updates, then send an update that's marked as 'charging'.
  // Make sure that the baseline resets.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  const int bucket = MakeRecordable();

  const float fake_baseline_level = 0.95;
  const float baseline_level = 0.9;
  const float second_level = baseline_level - 0.10;

  // Send the fake baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, fake_baseline_level);

  // Send an update that's marked as charging.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(true, second_level);

  // This should be the correct baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);

  // This should trigger recording.
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  FastForward(time_delta);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, second_level);
  VerifyHistogramDelta(bucket, 10, time_delta);
}

TEST_F(PowerStatusHelperImplTest, ExperimentStateStopsRecording) {
  // Verify that stopping the power experiment stops recording.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->UpdatePowerExperimentState(false);
  base::RunLoop().RunUntilIdle();

  // Call the callback to make sure nothing bad happens.  It should be ignored,
  // since it shouldn't use battery updates after the experiment stops.
  monitor_.ProvidePowerUpdate(false, 1.0);
}

TEST_F(PowerStatusHelperImplTest, ChangingBucketsWorks) {
  // Switch buckets mid-recording, and make sure that we get a new bucket and
  // use a new baseline.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  auto first_bucket = MakeRecordable(false);

  const float fake_baseline_level = 0.95;
  const float baseline_level = 0.9;
  const float second_level = baseline_level - 0.10;

  // Send the fake baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, fake_baseline_level);

  // Switch buckets.
  auto second_bucket = MakeRecordable(true);
  ASSERT_NE(first_bucket, second_bucket);

  // This should be the correct baseline.
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, baseline_level);

  // This should trigger recording.
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  FastForward(time_delta);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  monitor_.ProvidePowerUpdate(false, second_level);
  VerifyHistogramDelta(second_bucket, 10, time_delta);
}

TEST_F(PowerStatusHelperImplTest, UnbucketedVideoStopsRecording) {
  // If we switch to video that doesn't have a bucket, then recording should
  // stop too.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  // Should disconnect when we send bad params.
  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->SetIsPlaying(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PowerStatusHelperImplTest, UnbucketedFrameRateStopsRecording) {
  // If we switch to an unbucketed frame rate, then it should stop recording.
  EXPECT_CALL(monitor_, DidGetBatteryMonitor()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(1);
  MakeRecordable();

  // Should disconnect when we send bad params.
  EXPECT_CALL(monitor_, DidDisconnect()).Times(1);
  EXPECT_CALL(monitor_, DidQueryNextStatus()).Times(0);
  helper_->SetAverageFrameRate({});
  base::RunLoop().RunUntilIdle();
}

using PlaybackParamsTuple =
    std::tuple<bool,                        /* is_playing */
               bool,                        /* has_video */
               PowerStatusHelperImpl::Bits, /* codec */
               PowerStatusHelperImpl::Bits, /* resolution */
               PowerStatusHelperImpl::Bits, /* frame rate */
               PowerStatusHelperImpl::Bits  /* full screen */
               >;

class PowerStatusHelperImplBucketTest
    : public testing::TestWithParam<PlaybackParamsTuple> {
 public:
  base::Optional<int> BucketFor(bool is_playing,
                                bool has_video,
                                media::VideoCodec codec,
                                media::VideoCodecProfile profile,
                                gfx::Size coded_size,
                                bool is_fullscreen,
                                base::Optional<int> average_fps) {
    return PowerStatusHelperImpl::BucketFor(is_playing, has_video, codec,
                                            profile, coded_size, is_fullscreen,
                                            average_fps);
  }
};

TEST_P(PowerStatusHelperImplBucketTest, TestBucket) {
  // Construct a params that should end up in the bucket specified by the test
  // parameter, if one exists.
  bool expect_bucket = true;

  bool is_playing = std::get<0>(GetParam());
  bool has_video = std::get<1>(GetParam());

  // We must be playing video to get a bucket.
  if (!is_playing || !has_video)
    expect_bucket = false;

  auto codec_bits = std::get<2>(GetParam());
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
  if (codec_bits == PowerStatusHelperImpl::Bits::kCodecBitsH264) {
    codec = media::kCodecH264;
    profile = media::H264PROFILE_MAIN;
  } else if (codec_bits == PowerStatusHelperImpl::Bits::kCodecBitsVP9Profile0) {
    codec = media::kCodecVP9;
    profile = media::VP9PROFILE_PROFILE0;
  } else if (codec_bits == PowerStatusHelperImpl::Bits::kCodecBitsVP9Profile2) {
    codec = media::kCodecVP9;
    profile = media::VP9PROFILE_PROFILE2;
  } else {
    // Some unsupported codec.
    codec = media::kCodecVP8;
    profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    expect_bucket = false;
  }

  auto res = std::get<3>(GetParam());
  gfx::Size coded_size;
  if (res == PowerStatusHelperImpl::Bits::kResolution360p) {
    coded_size = gfx::Size(640, 360);
  } else if (res == PowerStatusHelperImpl::Bits::kResolution720p) {
    coded_size = gfx::Size(1280, 720);
  } else if (res == PowerStatusHelperImpl::Bits::kResolution1080p) {
    coded_size = gfx::Size(1920, 1080);
  } else {
    coded_size = gfx::Size(1234, 5678);
    expect_bucket = false;
  }

  auto fps = std::get<4>(GetParam());
  base::Optional<int> average_fps;
  if (fps == PowerStatusHelperImpl::Bits::kFrameRate30) {
    average_fps = 30;
  } else if (fps == PowerStatusHelperImpl::Bits::kFrameRate60) {
    average_fps = 60;
  } else {
    average_fps = 90;
    expect_bucket = false;
  }

  bool is_fullscreen =
      (std::get<5>(GetParam()) == PowerStatusHelperImpl::Bits::kFullScreenYes);

  auto bucket = BucketFor(is_playing, has_video, codec, profile, coded_size,
                          is_fullscreen, average_fps);
  if (!expect_bucket) {
    EXPECT_FALSE(bucket);
  } else {
    EXPECT_EQ(*bucket, std::get<2>(GetParam()) | std::get<3>(GetParam()) |
                           std::get<4>(GetParam()) | std::get<5>(GetParam()));
  }
}

// Instantiate all valid combinations, plus some that aren't.
INSTANTIATE_TEST_SUITE_P(
    All,
    PowerStatusHelperImplBucketTest,
    Combine(Bool(),
            Bool(),
            Values(PowerStatusHelperImpl::Bits::kCodecBitsH264,
                   PowerStatusHelperImpl::Bits::kCodecBitsVP9Profile0,
                   PowerStatusHelperImpl::Bits::kCodecBitsVP9Profile2,
                   PowerStatusHelperImpl::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelperImpl::Bits::kResolution360p,
                   PowerStatusHelperImpl::Bits::kResolution720p,
                   PowerStatusHelperImpl::Bits::kResolution1080p,
                   PowerStatusHelperImpl::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelperImpl::Bits::kFrameRate30,
                   PowerStatusHelperImpl::Bits::kFrameRate60,
                   PowerStatusHelperImpl::Bits::kNotAValidBitForTesting),
            Values(PowerStatusHelperImpl::Bits::kFullScreenNo,
                   PowerStatusHelperImpl::Bits::kFullScreenYes)));

}  // namespace content
