// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

namespace media {

webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters) {
  int channels = parameters.channels();

  // Mapping all discrete channel layouts to max two channels assuming that any
  // required channel remix takes place in the native audio layer.
  if (parameters.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE) {
    channels = std::min(parameters.channels(), 2);
  }
  const int rate = parameters.sample_rate();
  const bool has_keyboard = parameters.channel_layout() ==
                            media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;

  // webrtc::StreamConfig requires that the keyboard mic channel is not included
  // in the channel count. It may still be used.
  if (has_keyboard)
    channels -= 1;
  return webrtc::StreamConfig(rate, channels, has_keyboard);
}

bool LeftAndRightChannelsAreSymmetric(const AudioBus& audio) {
  if (audio.channels() <= 1) {
    return true;
  }
  return std::equal(audio.channel(0), audio.channel(0) + audio.frames(),
                    audio.channel(1));
}

}  // namespace media
