// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/media/api/cast_audio_resampler.h"
#include "media/base/audio_bus.h"
#include "media/base/multi_channel_resampler.h"

namespace chromecast {
namespace media {

namespace {

constexpr int kRequestFrames = 128;

class CastAudioResamplerImpl : public CastAudioResampler {
 public:
  CastAudioResamplerImpl(int channel_count,
                         int input_sample_rate,
                         int output_sample_rate)
      : channel_count_(channel_count),
        resampler_(channel_count,
                   static_cast<double>(input_sample_rate) / output_sample_rate,
                   kRequestFrames,
                   base::BindRepeating(&CastAudioResamplerImpl::ReadCallback,
                                       base::Unretained(this))),
        input_buffer_(::media::AudioBus::Create(channel_count, kRequestFrames)),
        output_buffer_(
            ::media::AudioBus::Create(channel_count, kRequestFrames * 2)) {}

  ~CastAudioResamplerImpl() override = default;

  CastAudioResamplerImpl(const CastAudioResamplerImpl&) = delete;
  CastAudioResamplerImpl& operator=(const CastAudioResamplerImpl&) = delete;

 private:
  void ResampleOneChunk(std::vector<float>* output_channels) {
    int output_frames = resampler_.ChunkSize();
    if (output_frames > output_buffer_->frames()) {
      output_buffer_ =
          ::media::AudioBus::Create(channel_count_, output_frames * 2);
    }
    resampler_.Resample(output_frames, output_buffer_.get());

    for (int c = 0; c < channel_count_; ++c) {
      const float* input_channel = output_buffer_->channel(c);
      output_channels[c].insert(output_channels[c].end(), input_channel,
                                input_channel + output_frames);
    }
  }

  void ReadCallback(int frame_delay, ::media::AudioBus* audio_bus) {
    DCHECK_LE(buffered_frames_, audio_bus->frames());
    input_buffer_->CopyPartialFramesTo(0, buffered_frames_, 0, audio_bus);
    int frames_left = audio_bus->frames() - buffered_frames_;
    int dest_offset = buffered_frames_;
    buffered_frames_ = 0;

    CopyCurrentInputTo(frames_left, audio_bus, dest_offset);
  }

  void CopyCurrentInputTo(int frames_to_copy,
                          ::media::AudioBus* dest,
                          int dest_offset) {
    DCHECK(current_input_);
    DCHECK_LE(dest_offset + frames_to_copy, dest->frames());
    DCHECK_LE(current_frame_offset_ + frames_to_copy, current_num_frames_);

    for (int c = 0; c < channel_count_; ++c) {
      const float* input_channel = current_input_ + c * current_num_frames_;
      std::copy_n(input_channel + current_frame_offset_, frames_to_copy,
                  dest->channel(c) + dest_offset);
    }
    current_frame_offset_ += frames_to_copy;
  }

  // CastAudioResampler implementation:
  void Resample(const float* input,
                int num_frames,
                std::vector<float>* output_channels) override {
    current_input_ = input;
    current_num_frames_ = num_frames;
    current_frame_offset_ = 0;

    while (buffered_frames_ + current_num_frames_ - current_frame_offset_ >=
           kRequestFrames) {
      ResampleOneChunk(output_channels);
    }

    int frames_left = current_num_frames_ - current_frame_offset_;
    CopyCurrentInputTo(frames_left, input_buffer_.get(), buffered_frames_);
    buffered_frames_ += frames_left;

    current_input_ = nullptr;
    current_num_frames_ = 0;
    current_frame_offset_ = 0;
  }

  void Flush(std::vector<float>* output_channels) override {
    // TODO(kmackay) May need some additional flushing to get out data stored in
    // the SincResamplers.
    if (buffered_frames_ == 0) {
      return;
    }

    input_buffer_->ZeroFramesPartial(buffered_frames_,
                                     kRequestFrames - buffered_frames_);
    buffered_frames_ = kRequestFrames;
    while (buffered_frames_) {
      ResampleOneChunk(output_channels);
    }

    resampler_.Flush();
  }

  int BufferedInputFrames() const override {
    return buffered_frames_ + std::round(resampler_.BufferedFrames());
  }

  const int channel_count_;

  ::media::MultiChannelResampler resampler_;

  std::unique_ptr<::media::AudioBus> input_buffer_;
  int buffered_frames_ = 0;

  const float* current_input_ = nullptr;
  int current_num_frames_ = 0;
  int current_frame_offset_ = 0;

  std::unique_ptr<::media::AudioBus> output_buffer_;
};

}  // namespace

// static
std::unique_ptr<CastAudioResampler> CastAudioResampler::Create(
    int channel_count,
    int input_sample_rate,
    int output_sample_rate) {
  return std::make_unique<CastAudioResamplerImpl>(
      channel_count, input_sample_rate, output_sample_rate);
}

}  // namespace media
}  // namespace chromecast
