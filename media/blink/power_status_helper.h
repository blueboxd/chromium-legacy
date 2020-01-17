// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_POWER_STATUS_HELPER_H_
#define MEDIA_BLINK_POWER_STATUS_HELPER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "media/base/pipeline_metadata.h"
#include "media/base/video_codecs.h"
#include "media/blink/media_blink_export.h"

namespace media {

// Base class to monitor for power events during playback and record them to
// UMA / UKM.
class MEDIA_BLINK_EXPORT PowerStatusHelper {
 public:
  PowerStatusHelper() = default;
  virtual ~PowerStatusHelper() = default;

  // Notify us about changes to the player.
  virtual void SetIsPlaying(bool is_playing) = 0;
  virtual void SetMetadata(const PipelineMetadata& metadata) = 0;
  virtual void SetIsFullscreen(bool is_fullscreen) = 0;
  virtual void SetAverageFrameRate(base::Optional<int> average_fps) = 0;

  // Handle notifications about the experiment state from the power experiment.
  // manager.  |state| indicates whether our player is eligible to record power
  // experiments readings.
  virtual void UpdatePowerExperimentState(bool state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerStatusHelper);
};

}  // namespace media

#endif  // MEDIA_BLINK_POWER_STATUS_HELPER_H_
