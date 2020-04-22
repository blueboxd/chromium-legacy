// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/metrics/fuchsia_playback_events_recorder.h"

#include "base/metrics/user_metrics.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr base::TimeDelta kSecond = base::TimeDelta::FromSeconds(1);

class FuchsiaPlaybackEventsRecorderTest : public testing::Test {
 public:
  FuchsiaPlaybackEventsRecorderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    time_base_ = base::TimeTicks::Now();

    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    action_callback_ = base::BindRepeating(
        &FuchsiaPlaybackEventsRecorderTest::OnAction, base::Unretained(this));
    base::AddActionCallback(action_callback_);
  }

  ~FuchsiaPlaybackEventsRecorderTest() override {
    base::RemoveActionCallback(action_callback_);
  }

 protected:
  struct Event {
    base::TimeTicks time;
    std::string name;

    bool operator==(const Event& other) const {
      return time == other.time && name == other.name;
    }
  };

  void OnAction(const std::string& name, base::TimeTicks time) {
    recorded_events_.push_back({time, name});
  }

  void ExpectEvents(const std::vector<Event>& expected) {
    EXPECT_EQ(recorded_events_.size(), expected.size());
    size_t end = std::min(recorded_events_.size(), expected.size());
    for (size_t i = 0; i < end; ++i) {
      SCOPED_TRACE(i);
      EXPECT_EQ(recorded_events_[i].time, expected[i].time);
      EXPECT_EQ(recorded_events_[i].name, expected[i].name);
    }
  }

  base::test::TaskEnvironment task_environment_;

  base::SimpleTestTickClock test_clock_;
  base::TimeTicks time_base_;

  base::ActionCallback action_callback_;
  FuchsiaPlaybackEventsRecorder recorder_;
  std::vector<Event> recorded_events_;
};

TEST_F(FuchsiaPlaybackEventsRecorderTest, PlayPause) {
  recorder_.OnNaturalSizeChanged(gfx::Size(640, 480));
  recorder_.OnPlaying();
  task_environment_.AdvanceClock(2 * kSecond);
  recorder_.OnPaused();

  ExpectEvents({
      {time_base_, "WebEngine.Media.VideoResolution:640x480"},
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 2 * kSecond, "WebEngine.Media.Pause"},
  });
}

TEST_F(FuchsiaPlaybackEventsRecorderTest, Error) {
  recorder_.OnPlaying();
  task_environment_.AdvanceClock(2 * kSecond);
  recorder_.OnError(PIPELINE_ERROR_DECODE);

  ExpectEvents({
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 2 * kSecond, "WebEngine.Media.Error:3"},
  });
}

TEST_F(FuchsiaPlaybackEventsRecorderTest, Buffering) {
  recorder_.OnPlaying();
  recorder_.OnBufferingComplete();
  task_environment_.AdvanceClock(2 * kSecond);
  recorder_.OnBuffering();
  task_environment_.AdvanceClock(3 * kSecond);
  recorder_.OnBufferingComplete();

  ExpectEvents({
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 5 * kSecond,
       "WebEngine.Media.PlayTimeBeforeAutoPause:2000"},
      {time_base_ + 5 * kSecond, "WebEngine.Media.AutoPauseTime:3000"},
  });
}

TEST_F(FuchsiaPlaybackEventsRecorderTest, Bitrate) {
  recorder_.OnPlaying();
  recorder_.OnBufferingComplete();

  PipelineStatistics stats;
  recorder_.OnPipelineStatistics(stats);

  for (int i = 0; i < 5; ++i) {
    stats.audio_bytes_decoded += 5000;
    stats.video_bytes_decoded += 10000;

    task_environment_.AdvanceClock(kSecond);
    recorder_.OnPipelineStatistics(stats);
  }

  ExpectEvents({
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 5 * kSecond, "WebEngine.Media.AudioBitrate:40"},
      {time_base_ + 5 * kSecond, "WebEngine.Media.VideoBitrate:80"},
  });
}

TEST_F(FuchsiaPlaybackEventsRecorderTest, BitrateAfterPause) {
  recorder_.OnPlaying();
  recorder_.OnBufferingComplete();

  PipelineStatistics stats;
  recorder_.OnPipelineStatistics(stats);

  for (int i = 0; i < 3; ++i) {
    stats.audio_bytes_decoded += 5000;
    stats.video_bytes_decoded += 10000;

    task_environment_.AdvanceClock(kSecond);
    recorder_.OnPipelineStatistics(stats);
  }

  recorder_.OnPaused();
  task_environment_.AdvanceClock(10 * kSecond);
  recorder_.OnPlaying();

  for (int i = 0; i < 3; ++i) {
    stats.audio_bytes_decoded += 5000;
    stats.video_bytes_decoded += 10000;

    task_environment_.AdvanceClock(kSecond);
    recorder_.OnPipelineStatistics(stats);
  }

  ExpectEvents({
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 3 * kSecond, "WebEngine.Media.Pause"},
      {time_base_ + 13 * kSecond, "WebEngine.Media.Playing"},
      {time_base_ + 16 * kSecond, "WebEngine.Media.AudioBitrate:40"},
      {time_base_ + 16 * kSecond, "WebEngine.Media.VideoBitrate:80"},
  });
}

TEST_F(FuchsiaPlaybackEventsRecorderTest, BitrateAfterBuffering) {
  recorder_.OnPlaying();
  recorder_.OnBufferingComplete();

  PipelineStatistics stats;
  recorder_.OnPipelineStatistics(stats);

  for (int i = 0; i < 3; ++i) {
    stats.audio_bytes_decoded += 5000;
    stats.video_bytes_decoded += 10000;

    task_environment_.AdvanceClock(kSecond);
    recorder_.OnPipelineStatistics(stats);
  }

  recorder_.OnBuffering();
  task_environment_.AdvanceClock(10 * kSecond);
  recorder_.OnBufferingComplete();

  for (int i = 0; i < 3; ++i) {
    stats.audio_bytes_decoded += 5000;
    stats.video_bytes_decoded += 10000;

    task_environment_.AdvanceClock(kSecond);
    recorder_.OnPipelineStatistics(stats);
  }

  ExpectEvents({
      {time_base_, "WebEngine.Media.Playing"},
      {time_base_ + 13 * kSecond,
       "WebEngine.Media.PlayTimeBeforeAutoPause:3000"},
      {time_base_ + 13 * kSecond, "WebEngine.Media.AutoPauseTime:10000"},
      {time_base_ + 16 * kSecond, "WebEngine.Media.AudioBitrate:40"},
      {time_base_ + 16 * kSecond, "WebEngine.Media.VideoBitrate:80"},
  });
}
}  // namespace media