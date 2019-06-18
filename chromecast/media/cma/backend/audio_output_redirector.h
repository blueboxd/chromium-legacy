// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_REDIRECTOR_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_REDIRECTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/redirected_audio_output.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {
class MixerInput;

// Empty interface so we can use a pointer to AudioOutputRedirector as the
// token.
class AudioOutputRedirectorToken {};

// The AudioOutputRedirector class determines which MixerInputs match the config
// conditions, and adds an AudioOutputRedirectorInput to each one.
// When the mixer is writing output, it tells each AudioOutputRedirector to
// create a new buffer (by calling PrepareNextBuffer()). As audio is pulled from
// each MixerInput, the resulting audio is passed through any added
// AudioOutputRedirectorInputs and is mixed into the redirected buffer by the
// AudioOutputRedirector (with appropriate fading to avoid pops/clicks, and
// volume applied if desired). Once all MixerInputs have been processed, the
// mixer will call FinishBuffer() on each AudioOutputRedirector; the
// redirected buffer is then sent to the RedirectedAudioOutput associated
// with each redirector.
class AudioOutputRedirector : public AudioOutputRedirectorToken {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  AudioOutputRedirector(const AudioOutputRedirectionConfig& config,
                        std::unique_ptr<RedirectedAudioOutput> output);
  ~AudioOutputRedirector();

  int order() const { return config_.order; }

  int64_t extra_delay_microseconds() const {
    return config_.extra_delay_microseconds;
  }

  // Adds/removes mixer inputs. The mixer adds/removes all mixer inputs from
  // the AudioOutputRedirector; this class does the work to determine which
  // inputs match the desired conditions.
  void AddInput(MixerInput* mixer_input);
  void RemoveInput(MixerInput* mixer_input);

  // Updates the set of patterns used to determine which inputs should be
  // redirected by this AudioOutputRedirector. Any inputs which no longer match
  // will stop being redirected.
  void UpdatePatterns(
      std::vector<std::pair<AudioContentType, std::string>> patterns);

  // Indicates that mixer output is starting at the given sample rate of
  // |output_samples_per_second|.
  void Start(int output_samples_per_second);

  // Indicates that mixer output is stopping.
  void Stop();

  // Called by the mixer when it is preparing to write another buffer of
  // |num_frames| frames.
  void PrepareNextBuffer(int num_frames);

  // Mixes audio into the redirected output bufer. Called by the
  // AudioOutputRedirectorInput implementation to mix in audio from each
  // matching MixerInput.
  void MixInput(MixerInput* mixer_input,
                ::media::AudioBus* data,
                int num_frames,
                RenderingDelay rendering_delay);

  // Called by the mixer once all MixerInputs have been processed; passes the
  // redirected audio buffer to the output plugin.
  void FinishBuffer();

 private:
  class InputImpl;

  bool ApplyToInput(MixerInput* mixer_input);

  AudioOutputRedirectionConfig config_;
  const std::unique_ptr<RedirectedAudioOutput> output_;
  int output_samples_per_second_ = 0;

  int next_num_frames_ = 0;
  int64_t next_output_timestamp_ = INT64_MIN;
  int input_count_ = 0;

  std::unique_ptr<::media::AudioBus> mixed_;
  std::vector<float*> channel_data_;

  base::flat_map<MixerInput*, std::unique_ptr<InputImpl>> inputs_;
  base::flat_set<MixerInput*> non_redirected_inputs_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputRedirector);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_REDIRECTOR_H_
