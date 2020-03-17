// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/video_playback_roughness_reporter.h"

#include <algorithm>
#include <random>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using VideoFrame = media::VideoFrame;
using VideoFrameMetadata = media::VideoFrameMetadata;

namespace cc {

class VideoPlaybackRoughnessReporterTest : public ::testing::Test {
 protected:
  std::unique_ptr<VideoPlaybackRoughnessReporter> reporter_;

  template <class T>
  void SetReportingCallabck(T cb) {
    reporter_ = std::make_unique<VideoPlaybackRoughnessReporter>(
        base::BindLambdaForTesting(cb));
  }

  VideoPlaybackRoughnessReporter* reporter() {
    DCHECK(reporter_);
    return reporter_.get();
  }

  scoped_refptr<VideoFrame> MakeFrame(base::TimeDelta duration) {
    scoped_refptr<VideoFrame> result = media::VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
    result->metadata()->SetTimeDelta(
        VideoFrameMetadata::WALLCLOCK_FRAME_DURATION, duration);
    return result;
  }

  ::testing::AssertionResult CheckSizes() {
    if (reporter()->frames_.size() > 80u)
      return ::testing::AssertionFailure()
             << "frames " << reporter()->frames_.size();
    if (reporter()->worst_windows_.size() > 20u)
      return ::testing::AssertionFailure()
             << "windows " << reporter()->worst_windows_.size();
    return ::testing::AssertionSuccess();
  }

  void NormalRun(double fps, double hz, std::vector<int> cadence, int frames) {
    base::TimeDelta vsync = base::TimeDelta::FromSecondsD(1 / hz);
    base::TimeDelta ideal_duration = base::TimeDelta::FromSecondsD(1 / fps);
    base::TimeTicks time;
    for (int idx = 0; idx < frames; idx++) {
      int frame_cadence = cadence[idx % cadence.size()];
      base::TimeDelta duration = vsync * frame_cadence;
      auto frame = MakeFrame(ideal_duration);
      reporter()->FrameSubmitted(idx, *frame, vsync);
      reporter()->FramePresented(idx, time);
      reporter()->ProcessFrameWindow();
      time += duration;
    }
  }

  void BatchPresentationRun(double fps,
                            double hz,
                            std::vector<int> cadence,
                            int frames) {
    base::TimeDelta vsync = base::TimeDelta::FromSecondsD(1 / hz);
    base::TimeDelta ideal_duration = base::TimeDelta::FromSecondsD(1 / fps);
    base::TimeTicks time;
    constexpr int batch_size = 3;
    for (int idx = 0; idx < frames; idx++) {
      auto frame = MakeFrame(ideal_duration);
      reporter()->FrameSubmitted(idx, *frame, vsync);

      if (idx % batch_size == batch_size - 1) {
        for (int i = batch_size - 1; i >= 0; i--) {
          int presented_idx = idx - i;
          int frame_cadence = cadence[presented_idx % cadence.size()];
          base::TimeDelta duration = vsync * frame_cadence;
          reporter()->FramePresented(presented_idx, time);
          time += duration;
        }
      }

      reporter()->ProcessFrameWindow();
    }
  }
};

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase24fps) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0118, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 10;
  NormalRun(fps, 60, {2, 3}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase24fpsOn120Hz) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 10;
  NormalRun(fps, 120, {5}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, Glitchy24fps) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0264, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 1;
  NormalRun(fps, 60, {2, 3, 1, 3, 2, 4, 2, 3, 2, 3, 3, 3}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase60fps) {
  int call_count = 0;
  int fps = 60;
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 1;
  NormalRun(fps, 60, {1}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase50fps) {
  int call_count = 0;
  int fps = 50;
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0163, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 1;
  NormalRun(fps, 60, {1, 1, 1, 1, 2}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

// Test that we understand the roughness algorithm by checking that we can
// get any result we need.
TEST_F(VideoPlaybackRoughnessReporterTest, PredictableRoughnessValue) {
  int fps = 12;
  int frames_in_window = fps / 2;
  int call_count = 0;
  double intended_roughness = 0.042;
  base::TimeDelta vsync = base::TimeDelta::FromSecondsD(1.0 / fps);
  // Calculating the error value that needs to be injected into one frame
  // in order to get desired roughness.
  base::TimeDelta error =
      std::sqrt(intended_roughness * intended_roughness * frames_in_window) *
      frames_in_window * vsync;

  auto callback = [&](int size, base::TimeDelta duration, double roughness) {
    ASSERT_EQ(frames_in_window, size);
    ASSERT_NEAR(roughness, intended_roughness, 0.0001);
    call_count++;
  };
  SetReportingCallabck(callback);
  int token = 0;
  int win_count = 50;
  for (int win_idx = 0; win_idx < win_count; win_idx++) {
    for (int frame_idx = 0; frame_idx < frames_in_window; frame_idx++) {
      base::TimeTicks time;
      time += token * vsync;
      if (frame_idx == frames_in_window - 1)
        time += error;

      auto frame = MakeFrame(vsync);
      reporter()->FrameSubmitted(token, *frame, vsync);
      reporter()->FramePresented(token++, time);
      reporter()->ProcessFrameWindow();
    }
  }
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that the reporter indeed takes 95% worst window.
TEST_F(VideoPlaybackRoughnessReporterTest, TakingPercentile) {
  int token = 0;
  int fps = 12;
  int frames_in_window = fps / 2;
  int call_count = 0;
  int win_count = 100;
  base::TimeDelta vsync = base::TimeDelta::FromSecondsD(1.0 / fps);
  std::vector<double> targets;
  targets.reserve(win_count);
  for (int i = 0; i < win_count; i++)
    targets.push_back(i * 0.0001);
  double expected_roughness =
      VideoPlaybackRoughnessReporter::kPercentileToSubmit * 0.0001;
  std::mt19937 rnd(1);
  std::shuffle(targets.begin(), targets.end(), rnd);

  auto callback = [&](int size, base::TimeDelta duration, double roughness) {
    ASSERT_EQ(frames_in_window, size);
    ASSERT_NEAR(roughness, expected_roughness, 0.00001);
    call_count++;
  };
  SetReportingCallabck(callback);

  for (int win_idx = 0; win_idx < win_count; win_idx++) {
    double roughness = targets[win_idx];
    // Calculating the error value that needs to be injected into one frame
    // in order to get desired roughness.
    base::TimeDelta error =
        std::sqrt(roughness * roughness * frames_in_window) * frames_in_window *
        vsync;

    for (int frame_idx = 0; frame_idx < frames_in_window; frame_idx++) {
      base::TimeTicks time;
      time += token * vsync;
      if (frame_idx == frames_in_window - 1)
        time += error;

      auto frame = MakeFrame(vsync);
      reporter()->FrameSubmitted(token, *frame, vsync);
      reporter()->FramePresented(token++, time);
      reporter()->ProcessFrameWindow();
    }
  }
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that even if no windows can be reported due to unstable presentation
// feedback, the reporter still doesn't run out of memory.
TEST_F(VideoPlaybackRoughnessReporterTest, LongRunWithoutWindows) {
  int call_count = 0;
  base::TimeDelta vsync = base::TimeDelta::FromMilliseconds(1);
  SetReportingCallabck([&](int size, base::TimeDelta duration,
                           double roughness) { call_count++; });
  for (int i = 0; i < 10000; i++) {
    auto frame = MakeFrame(vsync);
    reporter()->FrameSubmitted(i, *frame, vsync);
    if (i % 2 == 0)
      reporter()->FramePresented(i, base::TimeTicks() + i * vsync);
    reporter()->ProcessFrameWindow();
    ASSERT_TRUE(CheckSizes());
  }
  EXPECT_EQ(call_count, 0);
}

// Test that the reporter is no spooked by FramePresented() on unknown frame
// tokens.
TEST_F(VideoPlaybackRoughnessReporterTest, PresentingUnknownFrames) {
  int call_count = 0;
  base::TimeDelta vsync = base::TimeDelta::FromMilliseconds(1);
  SetReportingCallabck([&](int size, base::TimeDelta duration,
                           double roughness) { call_count++; });
  for (int i = 0; i < 10000; i++) {
    auto frame = MakeFrame(vsync);
    reporter()->FrameSubmitted(i, *frame, vsync);
    reporter()->FramePresented(i + 100000, base::TimeTicks() + i * vsync);
    reporter()->ProcessFrameWindow();
    ASSERT_TRUE(CheckSizes());
  }
  EXPECT_EQ(call_count, 0);
}

// Test that Reset() causes reporting if there is sufficient number of windows
// accumulated.
TEST_F(VideoPlaybackRoughnessReporterTest, ReportingInReset) {
  int call_count = 0;
  int fps = 60;
  auto callback = [&](int size, base::TimeDelta duration, double roughness) {
    call_count++;
  };
  SetReportingCallabck(callback);

  // Set number of frames insufficient for reporting in Reset()
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit * fps / 2 - 1;
  NormalRun(fps, 60, {1}, frames_to_run);
  // No calls since, not enough windows were reported
  EXPECT_EQ(call_count, 0);

  // Reset the reporter, still no calls
  reporter()->Reset();
  EXPECT_EQ(call_count, 0);

  // Set number of frames sufficient for reporting in Reset()
  frames_to_run =
      VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit * fps / 2 + 1;
  NormalRun(fps, 60, {1}, frames_to_run);

  // No calls since, not enough windows were reported
  EXPECT_EQ(call_count, 0);

  // A window should be reported in the Reset()
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that reporting works even if frame presentation signal come out of
// order.
TEST_F(VideoPlaybackRoughnessReporterTest, BatchPresentation) {
  int call_count = 0;
  int fps = 60;

  // Try 60 fps
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0, 0.0001);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 10;
  BatchPresentationRun(fps, 60, {1}, frames_to_run);
  EXPECT_EQ(call_count, 1);

  // Try 24fps
  SetReportingCallabck(
      [&](int size, base::TimeDelta duration, double roughness) {
        ASSERT_EQ(size, fps / 2);
        ASSERT_NEAR(duration.InMillisecondsF(), 500.0, 1.0);
        ASSERT_NEAR(roughness, 0.0118, 0.0001);
        call_count++;
      });
  fps = 24;
  frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps / 2 + 10;
  BatchPresentationRun(fps, 60, {2, 3}, frames_to_run);
  EXPECT_EQ(call_count, 2);
}

}  // namespace cc
