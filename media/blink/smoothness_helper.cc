// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/smoothness_helper.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "media/learning/common/learning_task_controller.h"

namespace {
static constexpr base::TimeDelta kSegmentSize =
    base::TimeDelta::FromSeconds(60);

// Max proportion of dropped frames in a window before we call it "not smooth".
static constexpr float kMaxDroppedFramesPerWindow = 0.2;
}

namespace media {

using learning::FeatureVector;
using learning::LearningTaskController;
using learning::TargetValue;

// Monitor smoothness during a playback, and call back on each window.
class SmoothnessWindowMonitor {
 public:
  using WindowCB = base::RepeatingCallback<void(int64_t dropped_frames,
                                                int64_t decoded_frames)>;
  SmoothnessWindowMonitor(SmoothnessHelper::Client* player, WindowCB cb)
      : player_(player), cb_(std::move(cb)) {
    segment_dropped_frames_ = player_->DroppedFrameCount();
    segment_decoded_frames_ = player_->DecodedFrameCount();

    update_timer_.Start(FROM_HERE, kSegmentSize,
                        base::BindRepeating(&SmoothnessWindowMonitor::OnTimer,
                                            base::Unretained(this)));
  }

  ~SmoothnessWindowMonitor() = default;

  // Split playback into segments of length |kSegmentSize|, and update the
  // default value of the current playback.
  void OnTimer() {
    auto new_dropped_frames = player_->DroppedFrameCount();
    auto dropped_frames = new_dropped_frames - segment_dropped_frames_;
    segment_dropped_frames_ = new_dropped_frames;

    auto new_decoded_frames = player_->DecodedFrameCount();
    auto decoded_frames = new_decoded_frames - segment_decoded_frames_;
    segment_decoded_frames_ = new_decoded_frames;

    if (!decoded_frames)
      return;

    cb_.Run(dropped_frames, decoded_frames);
  }

 private:
  SmoothnessHelper::Client* player_ = nullptr;
  WindowCB cb_;
  base::RepeatingTimer update_timer_;
  // Current dropped, decoded frames at the start of the segment.
  int64_t segment_decoded_frames_;
  int64_t segment_dropped_frames_;
};

class SmoothnessHelperImpl : public SmoothnessHelper {
 public:
  SmoothnessHelperImpl(
      std::unique_ptr<LearningTaskController> consecutive_controller,
      const FeatureVector& features,
      Client* player)
      : consecutive_bad_(std::move(consecutive_controller)),
        features_(features),
        player_(player) {
    monitor_ = std::make_unique<SmoothnessWindowMonitor>(
        player_, base::BindRepeating(&SmoothnessHelperImpl::OnWindow,
                                     base::Unretained(this)));
  }

  // This will ignore the last segment, if any, which is fine since it's not
  // a complete segment.  However, any in-progress observation will be completed
  // with the default value if we've gotten enough data to set one.
  ~SmoothnessHelperImpl() override = default;

  // Split playback into segments of length |kSegmentSize|, and update the
  // default value of the current playback.
  void OnWindow(int64_t dropped_frames, int64_t decoded_frames) {
    // Compute the percentage of dropped frames for this window.
    double pct = (static_cast<double>(dropped_frames)) / decoded_frames;

    // Once we get one full window, default to 0 for the consecutive windows
    // prediction task.
    if (!consecutive_bad_.is_started())
      consecutive_bad_.UpdateObservation(features_, TargetValue(0));

    // If this is a bad window, extend the run of consecutive bad windows, and
    // update the target value if this is a new longest run.
    if (pct >= kMaxDroppedFramesPerWindow) {
      consecutive_bad_windows_++;
      if (consecutive_bad_windows_ > max_consecutive_bad_windows_) {
        max_consecutive_bad_windows_ = consecutive_bad_windows_;
        consecutive_bad_.UpdateObservation(
            features_, TargetValue(max_consecutive_bad_windows_));
      }
    } else {
      consecutive_bad_windows_ = 0;
      // Don't update the target value, since any previous target value is still
      // the max consecutive windows.
    }
  }

  // Helper for different learning tasks.
  struct Task {
    Task(std::unique_ptr<LearningTaskController> controller)
        : controller_(std::move(controller)) {}

    ~Task() = default;

    // Return true if and only if we've started an observation.
    bool is_started() const { return !!id_; }

    void UpdateObservation(const FeatureVector& features,
                           TargetValue current_target) {
      target_value_ = current_target;
      if (!is_started()) {
        id_ = base::UnguessableToken::Create();
        controller_->BeginObservation(*id_, features, target_value_);
      } else {
        controller_->UpdateDefaultTarget(*id_, target_value_);
      }
    }

    const TargetValue& target_value() const { return target_value_; }

   private:
    // If an observation is in progress, then this is the id.
    base::Optional<base::UnguessableToken> id_;
    std::unique_ptr<LearningTaskController> controller_;
    TargetValue target_value_;

    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  // Struct to hold all of the "at least |n| consecutive bad windows" data.
  struct Task consecutive_bad_;

  int consecutive_bad_windows_ = 0;
  int max_consecutive_bad_windows_ = 0;

  FeatureVector features_;

  // WebMediaPlayer which will tell us about the decoded / dropped frame counts.
  Client* player_;

  std::unique_ptr<SmoothnessWindowMonitor> monitor_;
};

// static
std::unique_ptr<SmoothnessHelper> SmoothnessHelper::Create(
    std::unique_ptr<LearningTaskController> consecutive_controller,
    const FeatureVector& features,
    Client* player) {
  return std::make_unique<SmoothnessHelperImpl>(
      std::move(consecutive_controller), features, player);
}

// static
base::TimeDelta SmoothnessHelper::SegmentSizeForTesting() {
  return kSegmentSize;
}

}  // namespace media
