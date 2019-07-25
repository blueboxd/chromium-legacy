// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/stream_mixer.h"

#include <pthread.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/serializers.h"
#include "chromecast/base/thread_health_checker.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/audio_output_redirector.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "chromecast/media/cma/backend/filter_group.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_parser.h"
#include "chromecast/public/media/mixer_output_stream.h"
#include "media/audio/audio_device_description.h"

#define RUN_ON_MIXER_THREAD(method, ...)                                   \
  do {                                                                     \
    mixer_task_runner_->PostTask(                                          \
        FROM_HERE, base::BindOnce(&StreamMixer::method,                    \
                                  base::Unretained(this), ##__VA_ARGS__)); \
  } while (0)

#define MAKE_SURE_MIXER_THREAD(method, ...)                \
  if (!mixer_task_runner_->RunsTasksInCurrentSequence()) { \
    RUN_ON_MIXER_THREAD(method, ##__VA_ARGS__);            \
    return;                                                \
  }

namespace chromecast {
namespace media {

namespace {

const int kNumInputChannels = 2;

const int kDefaultCheckCloseTimeoutMs = 2000;

// Resample all audio below this frequency.
const unsigned int kLowSampleRateCutoff = 32000;

// Sample rate to fall back if input sample rate is below kLowSampleRateCutoff.
const unsigned int kLowSampleRateFallback = 48000;

const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

const int kUseDefaultFade = -1;
const int kMediaDuckFadeMs = 150;
const int kMediaUnduckFadeMs = 700;
const int kDefaultFilterFrameAlignment = 64;

constexpr base::TimeDelta kMixerThreadCheckTimeout =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kHealthCheckInterval =
    base::TimeDelta::FromSeconds(5);

int GetFixedOutputSampleRate() {
  int fixed_sample_rate = GetSwitchValueNonNegativeInt(
      switches::kAudioOutputSampleRate, MixerOutputStream::kInvalidSampleRate);

  if (fixed_sample_rate == MixerOutputStream::kInvalidSampleRate) {
    fixed_sample_rate =
        GetSwitchValueNonNegativeInt(switches::kAlsaFixedOutputSampleRate,
                                     MixerOutputStream::kInvalidSampleRate);
  }
  return fixed_sample_rate;
}

base::TimeDelta GetNoInputCloseTimeout() {
  // --accept-resource-provider should imply a check close timeout of 0.
  int default_close_timeout_ms =
      GetSwitchValueBoolean(switches::kAcceptResourceProvider, false)
          ? 0
          : kDefaultCheckCloseTimeoutMs;
  int close_timeout_ms = GetSwitchValueInt(switches::kAlsaCheckCloseTimeout,
                                           default_close_timeout_ms);
  if (close_timeout_ms < 0) {
    return base::TimeDelta::Max();
  }
  return base::TimeDelta::FromMilliseconds(close_timeout_ms);
}

void UseHighPriority() {
#if (!defined(OS_FUCHSIA) && !defined(OS_ANDROID))
  struct sched_param params;
  params.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);

  int policy = 0;
  struct sched_param actual_params;
  pthread_getschedparam(pthread_self(), &policy, &actual_params);
  LOG(INFO) << "Actual priority = " << actual_params.sched_priority
            << ", policy = " << policy;
#endif
}

}  // namespace

class StreamMixer::ExternalMediaVolumeChangeRequestObserver
    : public StreamMixer::BaseExternalMediaVolumeChangeRequestObserver {
 public:
  ExternalMediaVolumeChangeRequestObserver(StreamMixer* mixer) : mixer_(mixer) {
    DCHECK(mixer_);
  }

  // ExternalAudioPipelineShlib::ExternalMediaVolumeChangeRequestObserver
  // implementation:
  void OnVolumeChangeRequest(float new_volume) override {
    mixer_->SetVolume(AudioContentType::kMedia, new_volume);
  }

  void OnMuteChangeRequest(bool new_muted) override {
    mixer_->SetMuted(AudioContentType::kMedia, new_muted);
  }

 private:
  StreamMixer* const mixer_;
};

float StreamMixer::VolumeInfo::GetEffectiveVolume() {
  return std::min(volume, limit);
}

// static
StreamMixer* StreamMixer::Get() {
  static base::NoDestructor<StreamMixer> mixer_instance;
  return mixer_instance.get();
}

StreamMixer::StreamMixer()
    : StreamMixer(nullptr,
                  std::make_unique<base::Thread>("CMA mixer"),
                  nullptr) {}

StreamMixer::StreamMixer(
    std::unique_ptr<MixerOutputStream> output,
    std::unique_ptr<base::Thread> mixer_thread,
    scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner)
    : output_(std::move(output)),
      post_processing_pipeline_factory_(
          std::make_unique<PostProcessingPipelineFactoryImpl>()),
      mixer_thread_(std::move(mixer_thread)),
      mixer_task_runner_(std::move(mixer_task_runner)),
      loopback_handler_(LoopbackHandler::Create(mixer_task_runner_)),
      num_output_channels_(
          GetSwitchValueNonNegativeInt(switches::kAudioOutputChannels,
                                       kNumInputChannels)),
      low_sample_rate_cutoff_(
          GetSwitchValueBoolean(switches::kAlsaEnableUpsampling, false)
              ? kLowSampleRateCutoff
              : MixerOutputStream::kInvalidSampleRate),
      fixed_output_sample_rate_(GetFixedOutputSampleRate()),
      no_input_close_timeout_(GetNoInputCloseTimeout()),
      filter_frame_alignment_(kDefaultFilterFrameAlignment),
      state_(kStateStopped),
      external_audio_pipeline_supported_(
          ExternalAudioPipelineShlib::IsSupported()),
      weak_factory_(this) {
  LOG(INFO) << __func__;

  volume_info_[AudioContentType::kOther].volume = 1.0f;
  volume_info_[AudioContentType::kOther].limit = 1.0f;
  volume_info_[AudioContentType::kOther].muted = false;

  if (mixer_thread_) {
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
#if defined(OS_FUCHSIA)
    // MixerOutputStreamFuchsia uses FIDL, which works only on IO threads.
    options.message_loop_type = base::MessageLoop::TYPE_IO;
#endif
    mixer_thread_->StartWithOptions(options);
    mixer_task_runner_ = mixer_thread_->task_runner();
    mixer_task_runner_->PostTask(FROM_HERE, base::BindOnce(&UseHighPriority));

    health_checker_ = std::make_unique<ThreadHealthChecker>(
        mixer_task_runner_, loopback_handler_->GetTaskRunner(),
        kHealthCheckInterval, kMixerThreadCheckTimeout,
        base::BindRepeating(&StreamMixer::OnHealthCheckFailed,
                            base::Unretained(this)));
    LOG(INFO) << "Mixer health checker started";
  }

  if (fixed_output_sample_rate_ != MixerOutputStream::kInvalidSampleRate) {
    LOG(INFO) << "Setting fixed sample rate to " << fixed_output_sample_rate_;
  }

  CreatePostProcessors([](bool, const std::string&) {},
                       "" /* override_config */);
  mixer_pipeline_->SetPlayoutChannel(playout_channel_);

  // TODO(jyw): command line flag for filter frame alignment.
  DCHECK_EQ(filter_frame_alignment_ & (filter_frame_alignment_ - 1), 0)
      << "Alignment must be a power of 2.";

  if (external_audio_pipeline_supported_) {
    external_volume_observer_ =
        std::make_unique<ExternalMediaVolumeChangeRequestObserver>(this);
    ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
        external_volume_observer_.get());
  }
}

void StreamMixer::OnHealthCheckFailed() {
  LOG(FATAL) << "Crash on mixer thread health check failure!";
}

void StreamMixer::ResetPostProcessors(CastMediaShlib::ResultCallback callback) {
  RUN_ON_MIXER_THREAD(ResetPostProcessorsOnThread, std::move(callback), "");
}

void StreamMixer::ResetPostProcessorsOnThread(
    CastMediaShlib::ResultCallback callback,
    const std::string& override_config) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  // Detach inputs.
  for (const auto& input : inputs_) {
    input.second->SetFilterGroup(nullptr);
  }

  CreatePostProcessors(std::move(callback), override_config);

  // Re-attach inputs.
  for (const auto& input : inputs_) {
    FilterGroup* input_group =
        mixer_pipeline_->GetInputGroup(input.first->device_id());
    DCHECK(input_group) << "No input group for input.first->device_id()";
    input.second->SetFilterGroup(input_group);
  }
  UpdatePlayoutChannel();
}

// May be called on mixer_task_runner_ or from ctor
void StreamMixer::CreatePostProcessors(CastMediaShlib::ResultCallback callback,
                                       const std::string& override_config) {
  // (Re)-create post processors.
  mixer_pipeline_.reset();

  if (!override_config.empty()) {
    PostProcessingPipelineParser parser(
        base::DictionaryValue::From(DeserializeFromJson(override_config)));
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get());
  } else {
    PostProcessingPipelineParser parser(CastAudioJson::GetFilePath());
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get());
  }

  // Attempt to fall back to built-in cast_audio.json, unless we were reset with
  // an override config.
  if ((!mixer_pipeline_ || !PostProcessorsHaveCorrectNumOutputs()) &&
      override_config.empty()) {
    LOG(WARNING) << "Invalid cast_audio.json config loaded. Retrying with "
                    "read-only config";
    callback(false,
             "Unable to build pipeline.");  // TODO(bshaya): Send more specific
                                            // error message.
    callback = nullptr;
    PostProcessingPipelineParser parser(CastAudioJson::GetReadOnlyFilePath());
    mixer_pipeline_.reset();
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get());
  }

  CHECK(mixer_pipeline_) << "Unable to load post processor config!";
  UpdateLoopbackChannelCount();
  CHECK(PostProcessorsHaveCorrectNumOutputs());

  if (state_ == kStateRunning) {
    mixer_pipeline_->Initialize(output_samples_per_second_, frames_per_write_);
  }

  if (callback) {
    callback(true, "");
  }
}

void StreamMixer::UpdateLoopbackChannelCount() {
  loopback_channel_count_ = num_output_channels_ == 1
                                ? 1
                                : mixer_pipeline_->GetLoopbackChannelCount();
}

void StreamMixer::ResetPostProcessorsForTest(
    std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
    const std::string& pipeline_json) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __FUNCTION__ << " disregard previous PostProcessor messages.";
  mixer_pipeline_.reset();
  post_processing_pipeline_factory_ = std::move(pipeline_factory);
  ResetPostProcessorsOnThread([](bool, const std::string&) {}, pipeline_json);
}

void StreamMixer::SetNumOutputChannelsForTest(int num_output_channels) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  num_output_channels_ = num_output_channels;
  UpdateLoopbackChannelCount();
}

StreamMixer::~StreamMixer() {
  LOG(INFO) << __func__;

  mixer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StreamMixer::FinalizeOnMixerThread,
                                base::Unretained(this)));
  if (mixer_thread_) {
    mixer_thread_->Stop();
  }

  if (external_volume_observer_) {
    ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
        external_volume_observer_.get());
  }
}

void StreamMixer::FinalizeOnMixerThread() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  Stop();

  inputs_.clear();
  ignored_inputs_.clear();
}

void StreamMixer::Start() {
  LOG(INFO) << __func__;
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kStateStopped);
  DCHECK(inputs_.empty());

  if (!output_) {
    if (external_audio_pipeline_supported_) {
      output_ = ExternalAudioPipelineShlib::CreateMixerOutputStream();
    } else {
      output_ = MixerOutputStream::Create();
    }
  }
  DCHECK(output_);

  int requested_sample_rate;
  if (fixed_output_sample_rate_ != MixerOutputStream::kInvalidSampleRate) {
    requested_sample_rate = fixed_output_sample_rate_;
  } else if (low_sample_rate_cutoff_ != MixerOutputStream::kInvalidSampleRate &&
             requested_output_samples_per_second_ < low_sample_rate_cutoff_) {
    requested_sample_rate =
        output_samples_per_second_ != MixerOutputStream::kInvalidSampleRate
            ? output_samples_per_second_
            : kLowSampleRateFallback;
  } else {
    requested_sample_rate = requested_output_samples_per_second_;
  }

  if (!output_->Start(requested_sample_rate, num_output_channels_)) {
    Stop();
    return;
  }

  output_samples_per_second_ = output_->GetSampleRate();
  // Make sure the number of frames meets the filter alignment requirements.
  frames_per_write_ =
      output_->OptimalWriteFramesCount() & ~(filter_frame_alignment_ - 1);
  CHECK_GT(frames_per_write_, 0);

  loopback_handler_->SetDataSize(frames_per_write_ * loopback_channel_count_ *
                                 sizeof(float));

  // Initialize filters.
  mixer_pipeline_->Initialize(output_samples_per_second_, frames_per_write_);

  // Determine the appropriate sample rate for the redirector. If a product
  // needs to have these be different and support redirecting, then we will
  // need to add/update the per-input resamplers before redirecting.
  redirector_samples_per_second_ = GetSampleRateForDeviceId(
      ::media::AudioDeviceDescription::kDefaultDeviceId);

  const std::vector<const char*> redirectable_device_ids = {
      kPlatformAudioDeviceId, kAlarmAudioDeviceId, kTtsAudioDeviceId,
      ::media::AudioDeviceDescription::kDefaultDeviceId,
      ::media::AudioDeviceDescription::kCommunicationsDeviceId};

  for (const char* device_id : redirectable_device_ids) {
    DCHECK_EQ(redirector_samples_per_second_,
              GetSampleRateForDeviceId(device_id));
  }

  redirector_frames_per_write_ = redirector_samples_per_second_ *
                                 frames_per_write_ / output_samples_per_second_;
  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->Start(redirector_samples_per_second_);
  }

  state_ = kStateRunning;
  playback_loop_task_ = base::BindRepeating(&StreamMixer::PlaybackLoop,
                                            weak_factory_.GetWeakPtr());

  // Write one buffer of silence to get correct rendering delay in the
  // postprocessors.
  WriteOneBuffer();

  mixer_task_runner_->PostTask(FROM_HERE, playback_loop_task_);
}

void StreamMixer::Stop() {
  LOG(INFO) << __func__;
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  weak_factory_.InvalidateWeakPtrs();
  loopback_handler_->SendInterrupt();

  if (output_) {
    output_->Stop();
  }

  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->Stop();
  }

  state_ = kStateStopped;
  output_samples_per_second_ = MixerOutputStream::kInvalidSampleRate;
}

void StreamMixer::CheckChangeOutputRate(int input_samples_per_second) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ != kStateRunning ||
      input_samples_per_second == requested_output_samples_per_second_ ||
      input_samples_per_second == output_samples_per_second_ ||
      input_samples_per_second < static_cast<int>(low_sample_rate_cutoff_)) {
    return;
  }

  for (const auto& input : inputs_) {
    if (input.second->primary()) {
      return;
    }
  }

  // Ignore existing inputs.
  SignalError(MixerInput::Source::MixerError::kInputIgnored);

  requested_output_samples_per_second_ = input_samples_per_second;

  // Restart the output so that the new output sample rate takes effect.
  Stop();
  Start();
}

void StreamMixer::SignalError(MixerInput::Source::MixerError error) {
  // Move all current inputs to the ignored list and inform them of the error.
  for (auto& input : inputs_) {
    input.second->SignalError(error);
    ignored_inputs_.insert(std::move(input));
  }
  inputs_.clear();
  SetCloseTimeout();
}

void StreamMixer::AddInput(MixerInput::Source* input_source) {
  MAKE_SURE_MIXER_THREAD(AddInput, input_source);
  DCHECK(input_source);

  // If the new input is a primary one (or there were no inputs previously), we
  // may need to change the output sample rate to match the input sample rate.
  // We only change the output rate if it is not set to a fixed value.
  if ((input_source->primary() || inputs_.empty()) &&
      fixed_output_sample_rate_ == MixerOutputStream::kInvalidSampleRate) {
    CheckChangeOutputRate(input_source->input_samples_per_second());
  }

  if (state_ == kStateStopped) {
    requested_output_samples_per_second_ =
        input_source->input_samples_per_second();
    Start();
  }

  FilterGroup* input_group =
      mixer_pipeline_->GetInputGroup(input_source->device_id());
  DCHECK(input_group) << "Could not find a processor for "
                      << input_source->device_id();

  LOG(INFO) << "Add input " << input_source << " to " << input_group->name()
            << " @ " << input_group->GetInputSampleRate()
            << " samples per second.";

  auto input = std::make_unique<MixerInput>(input_source, input_group);
  if (state_ != kStateRunning) {
    // Mixer error occurred, signal error.
    MixerInput* input_ptr = input.get();
    ignored_inputs_[input_source] = std::move(input);
    input_ptr->SignalError(MixerInput::Source::MixerError::kInternalError);
    return;
  }

  auto type = input->content_type();
  if (type != AudioContentType::kOther) {
    if (input->primary()) {
      input->SetContentTypeVolume(volume_info_[type].GetEffectiveVolume(),
                                  kUseDefaultFade);
    } else {
      input->SetContentTypeVolume(volume_info_[type].volume, kUseDefaultFade);
    }
    input->SetMuted(volume_info_[type].muted);
  }

  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->AddInput(input.get());
  }

  inputs_[input_source] = std::move(input);
  UpdatePlayoutChannel();
}

void StreamMixer::RemoveInput(MixerInput::Source* input_source) {
  // Always post a task to avoid synchronous deletion.
  RUN_ON_MIXER_THREAD(RemoveInputOnThread, input_source);
}

void StreamMixer::RemoveInputOnThread(MixerInput::Source* input_source) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(input_source);

  LOG(INFO) << "Remove input " << input_source;

  auto it = inputs_.find(input_source);
  if (it != inputs_.end()) {
    for (auto& redirector : audio_output_redirectors_) {
      redirector.second->RemoveInput(it->second.get());
    }
    inputs_.erase(it);
  }

  ignored_inputs_.erase(input_source);
  UpdatePlayoutChannel();

  if (inputs_.empty()) {
    SetCloseTimeout();
  }
}

void StreamMixer::SetCloseTimeout() {
  close_timestamp_ = (no_input_close_timeout_.is_max()
                          ? base::TimeTicks::Max()
                          : base::TimeTicks::Now() + no_input_close_timeout_);
}

void StreamMixer::UpdatePlayoutChannel() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  int playout_channel;
  if (inputs_.empty()) {
    playout_channel = kChannelAll;
  } else {
    playout_channel = std::numeric_limits<int>::max();
    for (const auto& it : inputs_) {
      playout_channel =
          std::min(it.second->source()->playout_channel(), playout_channel);
    }
  }
  if (playout_channel == playout_channel_) {
    return;
  }

  DCHECK(playout_channel == kChannelAll ||
         playout_channel >= 0 && playout_channel < kNumInputChannels);
  LOG(INFO) << "Update playout channel: " << playout_channel;
  playout_channel_ = playout_channel;
  mixer_pipeline_->SetPlayoutChannel(playout_channel_);
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
StreamMixer::GetTotalRenderingDelay(FilterGroup* filter_group) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (!output_) {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }
  if (!filter_group) {
    return output_->GetRenderingDelay();
  }

  // Includes |output_->GetRenderingDelay()|.
  return filter_group->GetRenderingDelayToOutput();
}

void StreamMixer::PlaybackLoop() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (inputs_.empty() && base::TimeTicks::Now() >= close_timestamp_) {
    LOG(INFO) << "Close timeout";
    Stop();
    return;
  }

  WriteOneBuffer();

  mixer_task_runner_->PostTask(FROM_HERE, playback_loop_task_);
}

void StreamMixer::WriteOneBuffer() {
  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->PrepareNextBuffer(redirector_frames_per_write_);
  }

  // Recursively mix and filter each group.
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay =
      output_->GetRenderingDelay();
  mixer_pipeline_->MixAndFilter(frames_per_write_, rendering_delay);

  int64_t expected_playback_time;
  if (rendering_delay.timestamp_microseconds == kNoTimestamp) {
    expected_playback_time = kNoTimestamp;
  } else {
    expected_playback_time =
        rendering_delay.timestamp_microseconds +
        rendering_delay.delay_microseconds +
        mixer_pipeline_->GetPostLoopbackRenderingDelayMicroseconds();
  }

  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->FinishBuffer();
  }

  WriteMixedPcm(frames_per_write_, expected_playback_time);
}

void StreamMixer::WriteMixedPcm(int frames, int64_t expected_playback_time) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());

  // Downmix reference signal to mono to reduce CPU load.
  int mix_channel_count = mixer_pipeline_->GetLoopbackChannelCount();
  float* mixed_data = mixer_pipeline_->GetLoopbackOutput();
  if (loopback_channel_count_ == 1 && mix_channel_count != 1) {
    MixToMono(mixed_data, frames, mix_channel_count);
  }

  // Hard limit to [1.0, -1.0]
  for (int i = 0; i < frames * loopback_channel_count_; ++i) {
    // TODO(bshaya): Warn about clipping here.
    mixed_data[i] = std::min(1.0f, std::max(-1.0f, mixed_data[i]));
  }

  loopback_handler_->SendData(expected_playback_time,
                              output_samples_per_second_,
                              loopback_channel_count_, mixed_data, frames);

  // Drop extra channels from linearize filter if necessary.
  float* linearized_data = mixer_pipeline_->GetOutput();
  int linearize_channel_count = mixer_pipeline_->GetOutputChannelCount();
  if (num_output_channels_ == 1 && linearize_channel_count != 1) {
    MixToMono(linearized_data, frames, linearize_channel_count);
  }

  // Hard limit to [1.0, -1.0].
  for (int i = 0; i < frames * num_output_channels_; ++i) {
    linearized_data[i] = std::min(1.0f, std::max(-1.0f, linearized_data[i]));
  }

  bool playback_interrupted = false;
  output_->Write(linearized_data, frames * num_output_channels_,
                 &playback_interrupted);

  if (playback_interrupted) {
    loopback_handler_->SendInterrupt();
  }
}

void StreamMixer::MixToMono(float* data, int frames, int channels) {
  DCHECK_EQ(num_output_channels_, 1);
  if (playout_channel_ == kChannelAll) {
    for (int i = 0; i < frames; ++i) {
      float sum = 0;
      for (int c = 0; c < channels; ++c) {
        sum += data[i * channels + c];
      }
      data[i] = sum / channels;
    }
  } else {
    for (int i = 0; i < frames; ++i) {
      data[i] = data[i * channels + playout_channel_];
    }
  }
}

void StreamMixer::AddLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  loopback_handler_->AddObserver(observer);
}

void StreamMixer::RemoveLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  loopback_handler_->RemoveObserver(observer);
}

void StreamMixer::AddAudioOutputRedirector(
    std::unique_ptr<AudioOutputRedirector> redirector) {
  MAKE_SURE_MIXER_THREAD(AddAudioOutputRedirector, std::move(redirector));
  LOG(INFO) << __func__;
  DCHECK(redirector);

  AudioOutputRedirector* key = redirector.get();
  audio_output_redirectors_[key] = std::move(redirector);

  if (state_ == kStateRunning) {
    key->Start(redirector_samples_per_second_);
  }

  for (const auto& input : inputs_) {
    key->AddInput(input.second.get());
  }
}

void StreamMixer::RemoveAudioOutputRedirector(
    AudioOutputRedirector* redirector) {
  // Always post a task to avoid synchronous deletion.
  RUN_ON_MIXER_THREAD(RemoveAudioOutputRedirectorOnThread, redirector);
}

void StreamMixer::RemoveAudioOutputRedirectorOnThread(
    AudioOutputRedirector* redirector) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __func__;
  audio_output_redirectors_.erase(redirector);
}

void StreamMixer::ModifyAudioOutputRedirection(
    AudioOutputRedirector* redirector,
    std::vector<std::pair<AudioContentType, std::string>>
        stream_match_patterns) {
  MAKE_SURE_MIXER_THREAD(ModifyAudioOutputRedirection, redirector,
                         std::move(stream_match_patterns));

  auto it = audio_output_redirectors_.find(redirector);
  if (it != audio_output_redirectors_.end()) {
    it->second->UpdatePatterns(std::move(stream_match_patterns));
  }
}

void StreamMixer::SetVolume(AudioContentType type, float level) {
  MAKE_SURE_MIXER_THREAD(SetVolume, type, level);
  DCHECK(type != AudioContentType::kOther);

  volume_info_[type].volume = level;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  for (const auto& input : inputs_) {
    if (input.second->content_type() == type) {
      if (input.second->primary()) {
        input.second->SetContentTypeVolume(effective_volume, kUseDefaultFade);
      } else {
        // Volume limits don't apply to effects streams.
        input.second->SetContentTypeVolume(level, kUseDefaultFade);
      }
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaVolume(effective_volume);
  }
}

void StreamMixer::SetMuted(AudioContentType type, bool muted) {
  MAKE_SURE_MIXER_THREAD(SetMuted, type, muted);
  DCHECK(type != AudioContentType::kOther);

  volume_info_[type].muted = muted;
  for (const auto& input : inputs_) {
    if (input.second->content_type() == type) {
      input.second->SetMuted(muted);
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaMuted(muted);
  }
}

void StreamMixer::SetOutputLimit(AudioContentType type, float limit) {
  MAKE_SURE_MIXER_THREAD(SetOutputLimit, type, limit);
  DCHECK(type != AudioContentType::kOther);

  LOG(INFO) << "Set volume limit for " << static_cast<int>(type) << " to "
            << limit;
  volume_info_[type].limit = limit;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  int fade_ms = kUseDefaultFade;
  if (type == AudioContentType::kMedia) {
    if (limit >= 1.0f) {  // Unducking.
      fade_ms = kMediaUnduckFadeMs;
    } else {
      fade_ms = kMediaDuckFadeMs;
    }
  }
  for (const auto& input : inputs_) {
    // Volume limits don't apply to effects streams.
    if (input.second->primary() && input.second->content_type() == type) {
      input.second->SetContentTypeVolume(effective_volume, fade_ms);
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaVolume(effective_volume);
  }
}

void StreamMixer::SetVolumeMultiplier(MixerInput::Source* source,
                                      float multiplier) {
  MAKE_SURE_MIXER_THREAD(SetVolumeMultiplier, source, multiplier);

  auto it = inputs_.find(source);
  if (it != inputs_.end()) {
    it->second->SetVolumeMultiplier(multiplier);
  }
}

void StreamMixer::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  MAKE_SURE_MIXER_THREAD(SetPostProcessorConfig, name, config);

  mixer_pipeline_->SetPostProcessorConfig(name, config);
}

void StreamMixer::ValidatePostProcessorsForTest() {
  CHECK(PostProcessorsHaveCorrectNumOutputs());
}

bool StreamMixer::PostProcessorsHaveCorrectNumOutputs() {
  bool correct_num_outputs =
      num_output_channels_ == 1 ||
      num_output_channels_ == mixer_pipeline_->GetOutputChannelCount();
  if (!correct_num_outputs) {
    LOG(ERROR) << "PostProcessor configuration channel count does not match "
               << "command line flag: "
               << mixer_pipeline_->GetOutputChannelCount() << " vs "
               << num_output_channels_;
    return false;
  }
  if (loopback_channel_count_ > 2) {
    LOG(ERROR) << "PostProcessor configuration has " << loopback_channel_count_
               << " channels after 'mix' group, but only 1 or 2 are allowed.";
    return false;
  }
  return true;
}

int StreamMixer::GetSampleRateForDeviceId(const std::string& device) {
  DCHECK(mixer_pipeline_);
  return mixer_pipeline_->GetInputGroup(device)->GetInputSampleRate();
}

}  // namespace media
}  // namespace chromecast
