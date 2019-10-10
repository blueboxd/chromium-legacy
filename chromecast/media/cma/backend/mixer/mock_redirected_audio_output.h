// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "chromecast/public/media/redirected_audio_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

class MockRedirectedAudioOutput : public RedirectedAudioOutput {
 public:
  explicit MockRedirectedAudioOutput(int num_channels);
  ~MockRedirectedAudioOutput() override;

  int last_num_inputs() const { return last_num_inputs_; }
  ::media::AudioBus* last_buffer() const { return last_buffer_.get(); }
  int64_t last_output_timestamp() const { return last_output_timestamp_; }

  MOCK_METHOD1(Start, void(int));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(WriteBuffer, void(int, float**, int, int64_t));

 private:
  void OnWriteBuffer(int num_inputs,
                     float** channel_data,
                     int num_frames,
                     int64_t output_timestamp);

  const int num_channels_;

  int last_num_inputs_;
  std::unique_ptr<::media::AudioBus> last_buffer_;
  int64_t last_output_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(MockRedirectedAudioOutput);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_
