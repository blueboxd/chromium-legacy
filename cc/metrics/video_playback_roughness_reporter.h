// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_
#define CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"

namespace cc {

// Callback to report video playback roughness on a particularly bumpy interval.
// |frames| - number of frames in the interval
// |duration| - intended wallclock duration of the interval
// |roughness| - roughness of the interval
using PlaybackRoughnessReportingCallback = base::RepeatingCallback<
    void(int frames, base::TimeDelta duration, double roughness)>;

// This class tracks moments when each frame was submitted
// and when it was displayed. Then series of frames split into groups
// of consecutive frames, where each group takes about 1/2 second of playback.
// Such groups also called 'frame windows'. Each windows is assigned a roughness
// score that measures how far playback smoothness was from the ideal playback.
// Information about several windows and their roughness score is aggregated
// for a couple of playbackminutes and then a window with
// 95-percentile-max-roughness is reported via the provided callback.
// This sufficiently bad roughness window is deemed to represent overall
// playback quality.
class CC_EXPORT VideoPlaybackRoughnessReporter {
 public:
  using TokenType = uint32_t;
  explicit VideoPlaybackRoughnessReporter(
      PlaybackRoughnessReportingCallback reporting_cb);
  VideoPlaybackRoughnessReporter(const VideoPlaybackRoughnessReporter&) =
      delete;
  VideoPlaybackRoughnessReporter& operator=(
      const VideoPlaybackRoughnessReporter&) = delete;
  ~VideoPlaybackRoughnessReporter();
  void FrameSubmitted(TokenType token,
                      const media::VideoFrame& frame,
                      base::TimeDelta render_interval);
  void FramePresented(TokenType token, base::TimeTicks timestamp);
  void ProcessFrameWindow();
  void Reset();

  // A lower bund on how many frames can be in ConsecutiveFramesWindow
  static constexpr int kMinWindowSize = 6;

  // An upper bund on how many frames can be in ConsecutiveFramesWindow
  static constexpr int kMaxWindowSize = 40;

  // How many frame windows should be observed before reporting smoothness
  // due to playback time.
  // 1/2 a second per window, 200 windows. It means smoothness will be reported
  // for every 100 seconds of playback.
  static constexpr int kMaxWindowsBeforeSubmit = 200;

  // How many frame windows should be observed to report soothness on last
  // time before the destruction of the reporter.
  static constexpr int kMinWindowsBeforeSubmit = kMaxWindowsBeforeSubmit / 5;

  // A frame window with this percentile of playback roughness gets reported.
  // Lower value means more tolerance to rough playback stretches.
  static constexpr int kPercentileToSubmit = 95;
  static_assert(kPercentileToSubmit > 0 && kPercentileToSubmit < 100,
                "invalid percentile value");

 private:
  friend class VideoPlaybackRoughnessReporterTest;
  struct FrameInfo {
    FrameInfo();
    FrameInfo(const FrameInfo&);
    TokenType token = 0;
    base::Optional<base::TimeTicks> decode_time;
    base::Optional<base::TimeTicks> presentation_time;
    base::Optional<base::TimeDelta> actual_duration;
    base::Optional<base::TimeDelta> intended_duration;
  };

  struct ConsecutiveFramesWindow {
    int size;
    base::TimeTicks first_frame_time;
    base::TimeDelta intended_duration;

    // Worst case difference between a frame's intended duration and
    // actual duration, calculated for all frames in the window.
    base::TimeDelta max_single_frame_error;

    // Root-mean-square error of the differences between the intended
    // duration and the actual duration, calculated for all subwindows
    // starting at the beginning of the smoothness window
    // [1-2][1-3][1-4] ... [1-N].
    base::TimeDelta root_mean_square_error;

    double roughness() const;

    bool operator<(const ConsecutiveFramesWindow& rhs) const {
      double r1 = roughness();
      double r2 = rhs.roughness();
      if (r1 == r2) {
        // If roughnesses are equal use window start time as a tie breaker.
        // We don't want |flat_set worst_windows_| to dedup windows with
        // the same roughness.
        return first_frame_time > rhs.first_frame_time;
      }

      // Reverse sorting order to make sure that better windows go at the
      // end of |worst_windows_| set. This way it's cheaper to remove them.
      return r1 > r2;
    }
  };

  void ReportWindow(const ConsecutiveFramesWindow& win);
  void SubmitPlaybackRoughness();

  base::circular_deque<FrameInfo> frames_;
  base::flat_set<ConsecutiveFramesWindow> worst_windows_;
  PlaybackRoughnessReportingCallback reporting_cb_;
  int windows_seen_ = 0;
  int frames_window_size_ = kMinWindowSize;
};

}  // namespace cc

#endif  // CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_
