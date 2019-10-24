// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

namespace media {

webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters) {
  const int rate = parameters.sample_rate();
  const int channels = std::min(parameters.channels(), 2);
  const bool has_keyboard = parameters.channel_layout() ==
                            media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
  return webrtc::StreamConfig(rate, channels, has_keyboard);
}

}  // namespace media
