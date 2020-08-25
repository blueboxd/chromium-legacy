// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_FEEDBACK_H_
#define MEDIA_BASE_VIDEO_FRAME_FEEDBACK_H_

#include "base/optional.h"
#include "media/base/media_export.h"

namespace media {

// Feedback from the frames consumer.
// This class is passed from the frames sink to the capturer to limit
// incoming video feed frame-rate and/or resolution.
struct MEDIA_EXPORT VideoFrameFeedback {
  VideoFrameFeedback();
  VideoFrameFeedback(const VideoFrameFeedback& other);

  VideoFrameFeedback(base::Optional<double> resource_utilization,
                     float max_framerate_fps,
                     base::Optional<int> max_pixels);

  bool operator==(const VideoFrameFeedback& other) const {
    return resource_utilization == other.resource_utilization &&
           max_pixels == other.max_pixels &&
           max_framerate_fps == other.max_framerate_fps;
  }
  // Combine constraints of two different sinks resulting in constraints fitting
  // both of them.
  void Combine(const VideoFrameFeedback& other);

  // True if no actionable feedback is present (no resource utilization recorded
  // and all constraints are infinite or absent).
  bool Empty() const;

  // A feedback signal that indicates the fraction of the tolerable maximum
  // amount of resources that were utilized to process this frame.  A producer
  // can check this value after-the-fact, usually via a VideoFrame destruction
  // observer, to determine whether the consumer can handle more or less data
  // volume, and achieve the right quality versus performance trade-off.
  //
  // Values are interpreted as follows:
  // Less than 0.0 is meaningless and should be ignored.  1.0 indicates a
  // maximum sustainable utilization.  Greater than 1.0 indicates the consumer
  // is likely to stall or drop frames if the data volume is not reduced.
  //
  // Example: In a system that encodes and transmits video frames over the
  // network, this value can be used to indicate whether sufficient CPU
  // is available for encoding and/or sufficient bandwidth is available for
  // transmission over the network.  The maximum of the two utilization
  // measurements would be used as feedback.
  base::Optional<double> resource_utilization;

  // A feedback signal that indicates how big of a frame-rate and image size the
  // consumer can consume without overloading. A producer can check this value
  // after-the-fact, usually via a VideoFrame destruction observer, to
  // limit produced frame size and frame-rate accordingly.
  float max_framerate_fps = std::numeric_limits<float>::infinity();

  // Maximum requested resolution by a sink (given as a number of pixels).
  // -1 means no restriction.
  base::Optional<int> max_pixels;
};

}  // namespace media
#endif  // MEDIA_BASE_VIDEO_FRAME_FEEDBACK_H_
