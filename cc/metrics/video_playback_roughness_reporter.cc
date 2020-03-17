// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/video_playback_roughness_reporter.h"

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace {
constexpr int max_worst_windows_size() {
  constexpr int size =
      1 + cc::VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit *
              (100 - cc::VideoPlaybackRoughnessReporter::kPercentileToSubmit) /
              100;
  static_assert(size > 1, "worst_windows_ is too small");
  static_assert(size < 25, "worst_windows_ is too big");
  return size;
}

}  // namespace

namespace cc {

constexpr int VideoPlaybackRoughnessReporter::kMinWindowSize;
constexpr int VideoPlaybackRoughnessReporter::kMaxWindowSize;
constexpr int VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit;
constexpr int VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit;
constexpr int VideoPlaybackRoughnessReporter::kPercentileToSubmit;

VideoPlaybackRoughnessReporter::VideoPlaybackRoughnessReporter(
    PlaybackRoughnessReportingCallback reporting_cb)
    : reporting_cb_(reporting_cb) {}

VideoPlaybackRoughnessReporter::~VideoPlaybackRoughnessReporter() = default;

double VideoPlaybackRoughnessReporter::ConsecutiveFramesWindow::roughness()
    const {
  return root_mean_square_error.InMicrosecondsF() /
         intended_duration.InMicrosecondsF();
}

VideoPlaybackRoughnessReporter::FrameInfo::FrameInfo() = default;
VideoPlaybackRoughnessReporter::FrameInfo::FrameInfo(const FrameInfo&) =
    default;

void VideoPlaybackRoughnessReporter::FrameSubmitted(
    TokenType token,
    const media::VideoFrame& frame,
    base::TimeDelta render_interval) {
  if (!frames_.empty() && viz::FrameTokenGT(frames_.back().token, token)) {
    DCHECK(false) << "Frames submitted out of order.";
    return;
  }

  FrameInfo info;
  info.token = token;
  info.decode_time.emplace();
  if (!frame.metadata()->GetTimeTicks(
          media::VideoFrameMetadata::DECODE_END_TIME,
          &info.decode_time.value())) {
    info.decode_time.reset();
  }

  info.intended_duration.emplace();
  if (frame.metadata()->GetTimeDelta(
          media::VideoFrameMetadata::WALLCLOCK_FRAME_DURATION,
          &info.intended_duration.value())) {
    if (render_interval > info.intended_duration.value()) {
      // In videos with FPS higher than display refresh rate we acknowledge
      // the fact that some frames will be dropped upstream and frame's intended
      // duration can't be less than refresh interval.
      info.intended_duration = render_interval;
    }

    // Adjust frame window size to fit about 0.5 seconds of playback
    double win_size =
        std::round(0.5 / info.intended_duration.value().InSecondsF());
    frames_window_size_ = std::max(kMinWindowSize, static_cast<int>(win_size));
    frames_window_size_ = std::min(frames_window_size_, kMaxWindowSize);
  } else {
    info.intended_duration.reset();
  }
  frames_.push_back(info);
}

void VideoPlaybackRoughnessReporter::FramePresented(TokenType token,
                                                    base::TimeTicks timestamp) {
  for (auto it = frames_.rbegin(); it != frames_.rend(); it++) {
    FrameInfo& info = *it;
    if (token == it->token) {
      info.presentation_time = timestamp;
      if (info.decode_time.has_value()) {
        auto time_since_decode = timestamp - info.decode_time.value();
        UMA_HISTOGRAM_TIMES("Media.VideoFrameSubmitter", time_since_decode);
      }
      break;
    }
    if (viz::FrameTokenGT(token, it->token))
      break;
  }
}

void VideoPlaybackRoughnessReporter::SubmitPlaybackRoughness() {
  // 0-based index, how many times to step away from the begin().
  int index_to_submit = windows_seen_ * (100 - kPercentileToSubmit) / 100;
  if (index_to_submit < 0 ||
      index_to_submit >= static_cast<int>(worst_windows_.size())) {
    DCHECK(false);
    return;
  }

  auto it = worst_windows_.begin() + index_to_submit;
  reporting_cb_.Run(it->size, it->intended_duration, it->roughness());

  worst_windows_.clear();
  windows_seen_ = 0;
}

void VideoPlaybackRoughnessReporter::ReportWindow(
    const ConsecutiveFramesWindow& win) {
  if (win.max_single_frame_error >= win.intended_duration ||
      win.root_mean_square_error >= win.intended_duration) {
    // Most likely it's just an effect of the video being paused for some time.
    return;
  }
  worst_windows_.insert(win);
  if (worst_windows_.size() > max_worst_windows_size())
    worst_windows_.erase(std::prev(worst_windows_.end()));

  windows_seen_++;
  if (windows_seen_ >= kMaxWindowsBeforeSubmit)
    SubmitPlaybackRoughness();
}

void VideoPlaybackRoughnessReporter::ProcessFrameWindow() {
  if (static_cast<int>(frames_.size()) <= frames_window_size_) {
    // There is no window to speak of, let's wait and process it later.
    return;
  }

  // If possible populate duration for frames that don't have it yet.
  auto cur_frame_it = frames_.begin();
  auto next_frame_it = std::next(cur_frame_it);
  for (; next_frame_it != frames_.end(); cur_frame_it++, next_frame_it++) {
    FrameInfo& cur_frame = *cur_frame_it;
    const FrameInfo& next_frame = *next_frame_it;

    if (cur_frame.actual_duration.has_value())
      continue;

    if (!cur_frame.presentation_time.has_value() ||
        !next_frame.presentation_time.has_value()) {
      // We reached a frame that hasn't been presented yet, there is
      // no way to keep processing the window.
      break;
    }

    cur_frame.actual_duration = next_frame.presentation_time.value() -
                                cur_frame.presentation_time.value();
  }

  int items_to_discard = 0;
  const int max_buffer_size = 2 * frames_window_size_;
  // There is sufficient number of frames with populated |actual_duration|
  // let's calculate window metrics and report it.
  if (next_frame_it - frames_.begin() > frames_window_size_) {
    ConsecutiveFramesWindow win;
    double mean_square_error_ms2 = 0.0;
    base::TimeDelta total_error;
    if (frames_.front().presentation_time.has_value())
      win.first_frame_time = frames_.front().presentation_time.value();

    for (auto i = 0; i < frames_window_size_; i++) {
      FrameInfo& frame = frames_[i];
      base::TimeDelta error;

      if (frame.actual_duration.has_value() &&
          frame.intended_duration.has_value()) {
        error = frame.actual_duration.value() - frame.intended_duration.value();
        win.intended_duration += frame.intended_duration.value();
      }
      total_error += error;
      win.max_single_frame_error =
          std::max(win.max_single_frame_error, error.magnitude());
      mean_square_error_ms2 +=
          total_error.InMillisecondsF() * total_error.InMillisecondsF();
    }
    win.size = frames_window_size_;
    win.root_mean_square_error = base::TimeDelta::FromMillisecondsD(
        std::sqrt(mean_square_error_ms2 / frames_window_size_));

    ReportWindow(win);

    // The frames in the window have been reported,
    // no need to keep them around any longer.
    items_to_discard = frames_window_size_;
  } else if (static_cast<int>(frames_.size()) > max_buffer_size) {
    // |frames_| grew too much, because apparently we're not getting consistent
    // FramePresented() calls and no smoothness windows can be reported.
    // Nevertheless, we can't allow |frames_| to grow too big, let's drop
    // the oldest items beyond |max_buffer_size|;
    items_to_discard = frames_.size() - max_buffer_size;
  }

  frames_.erase(frames_.begin(), frames_.begin() + items_to_discard);
}

void VideoPlaybackRoughnessReporter::Reset() {
  if (windows_seen_ >= kMinWindowsBeforeSubmit)
    SubmitPlaybackRoughness();
  frames_.clear();
  worst_windows_.clear();
  windows_seen_ = 0;
}

}  // namespace cc
