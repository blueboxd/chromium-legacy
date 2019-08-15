// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CONTROL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CONTROL_H_

#include "chromecast/public/chromecast_export.h"

namespace chromecast {
namespace media {

// Interface for external control of mixer. The Get() method is only implemented
// when the mixer is actually present.
class CHROMECAST_EXPORT MixerControl {
 public:
  // If implemented, returns the control for the current mixer instance. The
  // returned pointer is valid until process shutdown.
  static MixerControl* Get() __attribute__((__weak__));

  // Sets the desired number of output channels used by the mixer. This will
  // cause an audio interruption on any currently active streams. The actual
  // output channel count is determined by the output implementation and may not
  // match |num_channels|.
  virtual void SetNumOutputChannels(int num_channels) = 0;

 protected:
  virtual ~MixerControl() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CONTROL_H_
