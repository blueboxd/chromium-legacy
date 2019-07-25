// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_output_redirector.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/strings/pattern.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/audio_fader.h"
#include "chromecast/media/cma/backend/audio_output_redirector_input.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/public/cast_media_shlib.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace chromecast {
namespace media {

namespace {
const int kDefaultBufferSize = 2048;
}  // namespace

class AudioOutputRedirector::InputImpl : public AudioOutputRedirectorInput {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  InputImpl(AudioOutputRedirector* output_redirector, MixerInput* mixer_input);
  ~InputImpl() override;

  // AudioOutputRedirectorInput implementation:
  int Order() override { return output_redirector_->order(); }
  int64_t GetDelayMicroseconds() override {
    return output_redirector_->extra_delay_microseconds();
  }
  void Redirect(::media::AudioBus* const buffer,
                int num_frames,
                RenderingDelay rendering_delay,
                bool redirected) override;

 private:
  AudioOutputRedirector* const output_redirector_;
  MixerInput* const mixer_input_;

  bool previous_ended_in_silence_;

  std::unique_ptr<::media::AudioBus> temp_buffer_;

  DISALLOW_COPY_AND_ASSIGN(InputImpl);
};

AudioOutputRedirector::InputImpl::InputImpl(
    AudioOutputRedirector* output_redirector,
    MixerInput* mixer_input)
    : output_redirector_(output_redirector),
      mixer_input_(mixer_input),
      previous_ended_in_silence_(true) {
  DCHECK(output_redirector_);
  DCHECK(mixer_input_);

  temp_buffer_ = ::media::AudioBus::Create(mixer_input_->num_channels(),
                                           kDefaultBufferSize);
  temp_buffer_->Zero();

  mixer_input_->AddAudioOutputRedirector(this);
}

AudioOutputRedirector::InputImpl::~InputImpl() {
  mixer_input_->RemoveAudioOutputRedirector(this);
}

void AudioOutputRedirector::InputImpl::Redirect(::media::AudioBus* const buffer,
                                                int num_frames,
                                                RenderingDelay rendering_delay,
                                                bool redirected) {
  if (!temp_buffer_ || temp_buffer_->frames() < num_frames) {
    temp_buffer_ = ::media::AudioBus::Create(mixer_input_->num_channels(),
                                             std::max(num_frames, 256));
  }

  if (num_frames != 0) {
    if (previous_ended_in_silence_ && redirected) {
      // Previous buffer ended in silence, and the current buffer was redirected
      // by a previous output splitter, so maintain silence.
      num_frames = 0;
    } else {
      buffer->CopyPartialFramesTo(0, num_frames, 0, temp_buffer_.get());
    }

    const int num_channels = buffer->channels();
    float* channels[num_channels];
    for (int c = 0; c < num_channels; ++c) {
      channels[c] = buffer->channel(c);
    }
    if (previous_ended_in_silence_) {
      if (!redirected) {
        // Smoothly fade in from previous silence.
        AudioFader::FadeInHelper(channels, num_channels, num_frames, num_frames,
                                 num_frames);
      }
    } else if (redirected) {
      // Smoothly fade out to silence, since output is now being redirected by a
      // previous output splitter.
      AudioFader::FadeOutHelper(channels, num_channels, num_frames, num_frames,
                                num_frames);
    }
    previous_ended_in_silence_ = redirected;
  }

  output_redirector_->MixInput(mixer_input_, temp_buffer_.get(), num_frames,
                               rendering_delay);
}

AudioOutputRedirector::AudioOutputRedirector(
    const AudioOutputRedirectionConfig& config,
    std::unique_ptr<RedirectedAudioOutput> output)
    : config_(config),
      output_(std::move(output)),
      channel_data_(config.num_output_channels) {
  DCHECK(output_);
  DCHECK_GT(config_.num_output_channels, 0);

  mixed_ = ::media::AudioBus::Create(config_.num_output_channels,
                                     kDefaultBufferSize);
  mixed_->Zero();
  for (int c = 0; c < config_.num_output_channels; ++c) {
    channel_data_[c] = mixed_->channel(c);
  }
}

AudioOutputRedirector::~AudioOutputRedirector() = default;

void AudioOutputRedirector::AddInput(MixerInput* mixer_input) {
  if (ApplyToInput(mixer_input)) {
    DCHECK_EQ(mixer_input->output_samples_per_second(),
              output_samples_per_second_);
    inputs_[mixer_input] = std::make_unique<InputImpl>(this, mixer_input);
  } else {
    non_redirected_inputs_.insert(mixer_input);
  }
}

void AudioOutputRedirector::RemoveInput(MixerInput* mixer_input) {
  inputs_.erase(mixer_input);
  non_redirected_inputs_.erase(mixer_input);
}

bool AudioOutputRedirector::ApplyToInput(MixerInput* mixer_input) {
  if (!mixer_input->primary()) {
    return false;
  }

  for (const auto& pattern : config_.stream_match_patterns) {
    if (mixer_input->content_type() == pattern.first &&
        base::MatchPattern(mixer_input->device_id(), pattern.second)) {
      return true;
    }
  }

  return false;
}

void AudioOutputRedirector::UpdatePatterns(
    std::vector<std::pair<AudioContentType, std::string>> patterns) {
  config_.stream_match_patterns = std::move(patterns);
  // Remove streams that no longer match.
  for (auto it = inputs_.begin(); it != inputs_.end();) {
    MixerInput* mixer_input = it->first;
    if (!ApplyToInput(mixer_input)) {
      non_redirected_inputs_.insert(mixer_input);
      it = inputs_.erase(it);
    } else {
      ++it;
    }
  }

  // Add streams that previously didn't match.
  for (auto it = non_redirected_inputs_.begin();
       it != non_redirected_inputs_.end();) {
    MixerInput* mixer_input = *it;
    if (ApplyToInput(mixer_input)) {
      inputs_[mixer_input] = std::make_unique<InputImpl>(this, mixer_input);
      it = non_redirected_inputs_.erase(it);
    } else {
      ++it;
    }
  }
}

void AudioOutputRedirector::Start(int output_samples_per_second) {
  output_samples_per_second_ = output_samples_per_second;
  output_->Start(output_samples_per_second);
}

void AudioOutputRedirector::Stop() {
  output_->Stop();
}

void AudioOutputRedirector::PrepareNextBuffer(int num_frames) {
  next_output_timestamp_ = INT64_MIN;
  next_num_frames_ = num_frames;

  if (!mixed_ || mixed_->frames() < num_frames) {
    mixed_ = ::media::AudioBus::Create(config_.num_output_channels, num_frames);
    for (int c = 0; c < config_.num_output_channels; ++c) {
      channel_data_[c] = mixed_->channel(c);
    }
  }
  mixed_->ZeroFramesPartial(0, num_frames);
  input_count_ = 0;
}

void AudioOutputRedirector::MixInput(MixerInput* mixer_input,
                                     ::media::AudioBus* data,
                                     int num_frames,
                                     RenderingDelay rendering_delay) {
  DCHECK_GE(mixed_->frames(), num_frames);
  DCHECK_GE(next_num_frames_, num_frames);

  if (rendering_delay.timestamp_microseconds != INT64_MIN) {
    int64_t output_timestamp = rendering_delay.timestamp_microseconds +
                               rendering_delay.delay_microseconds +
                               extra_delay_microseconds();
    if (next_output_timestamp_ == INT64_MIN ||
        output_timestamp < next_output_timestamp_) {
      next_output_timestamp_ = output_timestamp;
    }
  }

  if (num_frames <= 0) {
    return;
  }

  ++input_count_;
  int in_c = 0;
  for (int out_c = 0; out_c < config_.num_output_channels; ++out_c) {
    if (config_.apply_volume) {
      mixer_input->VolumeScaleAccumulate(data->channel(in_c), num_frames,
                                         mixed_->channel(out_c));
    } else {
      const float* temp_channel = data->channel(in_c);
      float* mixed_channel = mixed_->channel(out_c);
      for (int i = 0; i < num_frames; ++i) {
        mixed_channel[i] += temp_channel[i];
      }
    }

    ++in_c;
    if (in_c >= data->channels()) {
      in_c = 0;
    }
  }
}

void AudioOutputRedirector::FinishBuffer() {
  // Hard limit to [1.0, -1.0].
  for (int c = 0; c < config_.num_output_channels; ++c) {
    for (int f = 0; f < next_num_frames_; ++f) {
      channel_data_[c][f] =
          base::ClampToRange(channel_data_[c][f], -1.0f, 1.0f);
    }
  }

  output_->WriteBuffer(input_count_, channel_data_.data(), next_num_frames_,
                       next_output_timestamp_);
}

// static
AudioOutputRedirectorToken* CastMediaShlib::AddAudioOutputRedirection(
    const AudioOutputRedirectionConfig& config,
    std::unique_ptr<RedirectedAudioOutput> output) {
  if (!output || config.num_output_channels <= 0) {
    return nullptr;
  }

  auto redirector =
      std::make_unique<AudioOutputRedirector>(config, std::move(output));
  AudioOutputRedirectorToken* token = redirector.get();
  StreamMixer::Get()->AddAudioOutputRedirector(std::move(redirector));
  return token;
}

// static
void CastMediaShlib::RemoveAudioOutputRedirection(
    AudioOutputRedirectorToken* token) {
  AudioOutputRedirector* redirector =
      static_cast<AudioOutputRedirector*>(token);
  if (redirector) {
    StreamMixer::Get()->RemoveAudioOutputRedirector(redirector);
  }
}

// static
void CastMediaShlib::ModifyAudioOutputRedirection(
    AudioOutputRedirectorToken* token,
    std::vector<std::pair<AudioContentType, std::string>>
        stream_match_patterns) {
  AudioOutputRedirector* redirector =
      static_cast<AudioOutputRedirector*>(token);
  if (redirector) {
    StreamMixer::Get()->ModifyAudioOutputRedirection(
        redirector, std::move(stream_match_patterns));
  }
}

}  // namespace media
}  // namespace chromecast
