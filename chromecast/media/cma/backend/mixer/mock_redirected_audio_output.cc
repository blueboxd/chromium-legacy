// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mock_redirected_audio_output.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_bus.h"

using testing::_;

namespace chromecast {
namespace media {

MockRedirectedAudioOutput::MockRedirectedAudioOutput(int num_channels)
    : num_channels_(num_channels),
      last_num_inputs_(-1),
      last_output_timestamp_(INT64_MIN) {
  ON_CALL(*this, WriteBuffer(_, _, _, _))
      .WillByDefault(
          testing::Invoke(this, &MockRedirectedAudioOutput::OnWriteBuffer));
}

MockRedirectedAudioOutput::~MockRedirectedAudioOutput() = default;

void MockRedirectedAudioOutput::OnWriteBuffer(int num_inputs,
                                              float** channel_data,
                                              int num_frames,
                                              int64_t output_timestamp) {
  CHECK(channel_data);
  last_buffer_ = ::media::AudioBus::Create(num_channels_, num_frames);
  for (int c = 0; c < num_channels_; ++c) {
    CHECK(channel_data[c]);
    std::copy_n(channel_data[c], num_frames, last_buffer_->channel(c));
  }

  last_num_inputs_ = num_inputs;
  last_output_timestamp_ = output_timestamp;
}

}  // namespace media
}  // namespace chromecast
