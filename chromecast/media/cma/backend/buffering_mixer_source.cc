// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/buffering_mixer_source.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

const int64_t kDefaultInputQueueMs = 90;
constexpr base::TimeDelta kFadeTime = base::TimeDelta::FromMilliseconds(5);
const int kDefaultAudioReadyForPlaybackThresholdMs = 70;

// Special queue size and start threshold for "communications" streams to avoid
// issues with voice calling.
const int64_t kCommsInputQueueMs = 200;
const int64_t kCommsStartThresholdMs = 150;

const int kFreeBufferListSize = 64;

std::string AudioContentTypeToString(media::AudioContentType type) {
  switch (type) {
    case media::AudioContentType::kAlarm:
      return "alarm";
    case media::AudioContentType::kCommunication:
      return "communication";
    default:
      return "media";
  }
}

int MsToSamples(int64_t ms, int sample_rate) {
  return ::media::AudioTimestampHelper::TimeToFrames(
      base::TimeDelta::FromMilliseconds(ms), sample_rate);
}

int64_t SamplesToMicroseconds(int64_t samples, int sample_rate) {
  return ::media::AudioTimestampHelper::FramesToTime(samples, sample_rate)
      .InMicroseconds();
}

int MaxQueuedFrames(const std::string& device_id, int sample_rate) {
  int64_t queue_ms = kDefaultInputQueueMs;

  if (device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
    queue_ms = kCommsInputQueueMs;
  }

  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMixerSourceInputQueueMs)) {
    queue_ms = GetSwitchValueNonNegativeInt(switches::kMixerSourceInputQueueMs,
                                            queue_ms);
  }

  return MsToSamples(queue_ms, sample_rate);
}

int StartThreshold(const std::string& device_id, int sample_rate) {
  int64_t start_threshold_ms = kDefaultAudioReadyForPlaybackThresholdMs;

  if (device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
    start_threshold_ms = kCommsStartThresholdMs;
  }

  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMixerSourceAudioReadyThresholdMs)) {
    start_threshold_ms = GetSwitchValueNonNegativeInt(
        switches::kMixerSourceAudioReadyThresholdMs, start_threshold_ms);
  }

  return MsToSamples(start_threshold_ms, sample_rate);
}

}  // namespace

BufferingMixerSource::BufferingMixerSource(Delegate* delegate,
                                           int num_channels,
                                           int input_samples_per_second,
                                           bool primary,
                                           const std::string& device_id,
                                           AudioContentType content_type,
                                           int playout_channel,
                                           int64_t playback_start_pts,
                                           bool start_playback_asap)
    : delegate_(delegate),
      num_channels_(num_channels),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      device_id_(device_id),
      content_type_(content_type),
      playout_channel_(playout_channel),
      mixer_(StreamMixer::Get()),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      max_queued_frames_(MaxQueuedFrames(device_id, input_samples_per_second)),
      start_threshold_frames_(
          StartThreshold(device_id, input_samples_per_second)),
      fader_(this,
             kFadeTime,
             num_channels_,
             input_samples_per_second_,
             1.0 /* playback_rate */),
      playback_start_timestamp_(start_playback_asap ? INT64_MIN : INT64_MAX),
      playback_start_pts_(playback_start_pts),
      audio_resampler_(num_channels_),
      weak_factory_(this) {
  LOG(INFO) << "Create " << device_id_ << " (" << this
            << "), content type = " << AudioContentTypeToString(content_type_)
            << ", playback_start_pts=" << playback_start_pts;
  DCHECK_GT(num_channels_, 0);
  DCHECK(delegate_);
  DCHECK(mixer_);
  DCHECK_LE(start_threshold_frames_, max_queued_frames_);

  weak_this_ = weak_factory_.GetWeakPtr();
  buffers_to_be_freed_.reserve(kFreeBufferListSize);
  old_buffers_to_be_freed_.reserve(kFreeBufferListSize);

  pcm_completion_task_ =
      base::BindRepeating(&BufferingMixerSource::PostPcmCompletion, weak_this_);
  eos_task_ = base::BindRepeating(&BufferingMixerSource::PostEos, weak_this_);
  ready_for_playback_task_ = base::BindRepeating(
      &BufferingMixerSource::PostAudioReadyForPlayback, weak_this_);

  mixer_->AddInput(this);
}

BufferingMixerSource::~BufferingMixerSource() {
  LOG(INFO) << "Destroy " << device_id_ << " (" << this << ")";
}

void BufferingMixerSource::StartPlaybackAt(int64_t playback_start_timestamp) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_ready_for_playback_fired_);
  LOG(INFO) << __func__
            << " playback_start_timestamp=" << playback_start_timestamp;

  base::AutoLock lock(lock_);
  DCHECK(!started_);
  DCHECK_EQ(playback_start_timestamp_, INT64_MAX);
  playback_start_timestamp_ = playback_start_timestamp;
}

void BufferingMixerSource::RestartPlaybackAt(int64_t timestamp, int64_t pts) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(INFO) << __func__ << " timestamp=" << timestamp << " pts=" << pts;
  {
    base::AutoLock lock(lock_);
    DCHECK(started_);

    playback_start_pts_ = pts;
    playback_start_timestamp_ = timestamp;
    started_ = false;
    queued_frames_ += current_buffer_offset_;
    current_buffer_offset_ = 0;

    while (!queue_.empty()) {
      DecoderBufferBase* data = queue_.front().get();
      int64_t frames = DataToFrames(data->data_size());
      if (data->timestamp() +
              SamplesToMicroseconds(frames, input_samples_per_second_) >=
          pts) {
        break;
      }

      queued_frames_ -= frames;
      queue_.pop_front();
    }
  }
}

void BufferingMixerSource::SetMediaPlaybackRate(double rate) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __func__ << " rate=" << rate;
  DCHECK_GT(rate, 0);

  base::AutoLock lock(lock_);
  playback_rate_ = rate;
}

float BufferingMixerSource::SetAvSyncPlaybackRate(float rate) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __func__ << " rate=" << rate;

  base::AutoLock lock(lock_);
  return audio_resampler_.SetMediaClockRate(rate);
}

BufferingMixerSource::RenderingDelay
BufferingMixerSource::GetMixerRenderingDelay() {
  base::AutoLock lock(lock_);
  return mixer_rendering_delay_;
}

int BufferingMixerSource::num_channels() {
  return num_channels_;
}
int BufferingMixerSource::input_samples_per_second() {
  return input_samples_per_second_;
}
bool BufferingMixerSource::primary() {
  return primary_;
}
const std::string& BufferingMixerSource::device_id() {
  return device_id_;
}
AudioContentType BufferingMixerSource::content_type() {
  return content_type_;
}
int BufferingMixerSource::desired_read_size() {
  return input_samples_per_second_ / 100;
}
int BufferingMixerSource::playout_channel() {
  return playout_channel_;
}

void BufferingMixerSource::WritePcm(scoped_refptr<DecoderBufferBase> data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  RenderingDelay delay;
  bool queued;
  {
    base::AutoLock lock(lock_);
    old_buffers_to_be_freed_.swap(buffers_to_be_freed_);
    if (state_ == State::kUninitialized ||
        queued_frames_ + fader_.buffered_frames() >= max_queued_frames_) {
      DCHECK(!pending_data_);
      pending_data_ = std::move(data);
      queued = false;
    } else {
      delay = QueueData(std::move(data));
      queued = true;
    }
  }
  old_buffers_to_be_freed_.clear();
  if (queued) {
    delegate_->OnWritePcmCompletion(delay);
  }
}

BufferingMixerSource::RenderingDelay BufferingMixerSource::QueueData(
    scoped_refptr<DecoderBufferBase> data) {
  if (data->end_of_stream()) {
    LOG(INFO) << "End of stream for " << device_id_ << " (" << this << ")";
    state_ = State::kGotEos;
    if (!started_ && playback_start_timestamp_ != INT64_MIN) {
      caller_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  } else if (started_ ||
             data->timestamp() +
                     SamplesToMicroseconds(DataToFrames(data->data_size()),
                                           input_samples_per_second_) >=
                 playback_start_pts_) {
    scoped_refptr<DecoderBufferBase> buffer =
        audio_resampler_.ResampleBuffer(std::move(data));
    queued_frames_ += DataToFrames(buffer->data_size());
    queue_.push_back(std::move(buffer));

    if (!started_ && queued_frames_ >= start_threshold_frames_ &&
        playback_start_timestamp_ != INT64_MIN) {
      caller_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  }
  // Otherwise, drop |data| since it is before the start PTS.

  RenderingDelay delay;
  if (started_ && !paused_) {
    delay = mixer_rendering_delay_;
    delay.delay_microseconds += SamplesToMicroseconds(
        queued_frames_ + extra_delay_frames_, input_samples_per_second_);
  }
  return delay;
}

void BufferingMixerSource::SetPaused(bool paused) {
  LOG(INFO) << (paused ? "Pausing " : "Unpausing ") << device_id_ << " ("
            << this << ")";
  base::AutoLock lock(lock_);
  // Clear start timestamp, since a pause should invalidate the start timestamp
  // anyway. The AV sync code can restart (hard correction) on resume if
  // needed.
  playback_start_timestamp_ = INT64_MIN;
  mixer_rendering_delay_ = RenderingDelay();
  paused_ = paused;
}

void BufferingMixerSource::SetVolumeMultiplier(float multiplier) {
  mixer_->SetVolumeMultiplier(this, multiplier);
}

void BufferingMixerSource::InitializeAudioPlayback(
    int read_size,
    RenderingDelay initial_rendering_delay) {
  // Start accepting buffers into the queue.
  bool queued_data = false;
  {
    base::AutoLock lock(lock_);
    mixer_rendering_delay_ = initial_rendering_delay;
    if (state_ == State::kUninitialized) {
      state_ = State::kNormalPlayback;
    } else {
      DCHECK_EQ(state_, State::kRemoved);
    }

    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      last_buffer_delay_ = QueueData(std::move(pending_data_));
      queued_data = true;
    }
  }

  if (queued_data) {
    caller_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
}

void BufferingMixerSource::CheckAndStartPlaybackIfNecessary(
    int num_frames,
    int64_t playback_absolute_timestamp) {
  DCHECK(state_ == State::kNormalPlayback || state_ == State::kGotEos);
  DCHECK(!started_);

  const bool have_enough_queued_frames =
      (state_ == State::kGotEos ||
       (queued_frames_ >= start_threshold_frames_ &&
        queued_frames_ >= fader_.FramesNeededFromSource(num_frames)));
  if (!have_enough_queued_frames) {
    return;
  }

  remaining_silence_frames_ = 0;
  if (playback_start_timestamp_ == INT64_MIN ||
      (queue_.empty() && state_ == State::kGotEos)) {
    // No start timestamp, so start as soon as there are enough queued frames.
    started_ = true;
    return;
  }

  if (playback_absolute_timestamp +
          SamplesToMicroseconds(num_frames, input_samples_per_second_) <
      playback_start_timestamp_) {
    // Haven't reached the start timestamp yet.
    return;
  }

  DCHECK(!queue_.empty());
  // Reset the current buffer offset to 0 so we can ignore it below. We need to
  // do this here because we may not have started playback even after dropping
  // all necessary frames the last time we checked.
  queued_frames_ += current_buffer_offset_;
  current_buffer_offset_ = 0;

  int64_t desired_pts_now = playback_start_pts_ + (playback_absolute_timestamp -
                                                   playback_start_timestamp_) *
                                                      playback_rate_;
  int64_t actual_pts_now = queue_.front()->timestamp();
  int64_t drop_us = (desired_pts_now - actual_pts_now) / playback_rate_;

  if (drop_us >= 0) {
    LOG(INFO) << "Dropping audio, duration = " << drop_us;
    DropAudio(::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(drop_us), input_samples_per_second_));
    // Only start if we still have enough data to do so.
    started_ = (queued_frames_ >= start_threshold_frames_ &&
                queued_frames_ >= fader_.FramesNeededFromSource(num_frames));

    if (started_) {
      int64_t start_pts = queue_.front()->timestamp() +
                          SamplesToMicroseconds(current_buffer_offset_,
                                                input_samples_per_second_) *
                              playback_rate_;
      LOG(INFO) << "Start playback of PTS " << start_pts << " at "
                << playback_absolute_timestamp;
    }
  } else {
    int64_t silence_duration = -drop_us;
    LOG(INFO) << "Adding silence. Duration = " << silence_duration;
    remaining_silence_frames_ = ::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(silence_duration),
        input_samples_per_second_);
    started_ = true;
    LOG(INFO) << "Should start playback of PTS " << queue_.front()->timestamp()
              << " at " << (playback_absolute_timestamp + silence_duration);
  }
}

void BufferingMixerSource::DropAudio(int64_t frames_to_drop) {
  DCHECK_EQ(current_buffer_offset_, 0);

  while (frames_to_drop && !queue_.empty()) {
    int64_t first_buffer_frames = DataToFrames(queue_.front()->data_size());
    if (first_buffer_frames > frames_to_drop) {
      current_buffer_offset_ = frames_to_drop;
      queued_frames_ -= frames_to_drop;
      frames_to_drop = 0;
      break;
    }

    queued_frames_ -= first_buffer_frames;
    frames_to_drop -= first_buffer_frames;
    queue_.pop_front();
  }

  if (frames_to_drop > 0) {
    LOG(INFO) << "Still need to drop " << frames_to_drop << " frames";
  }
}

int BufferingMixerSource::FillAudioPlaybackFrames(
    int num_frames,
    RenderingDelay rendering_delay,
    ::media::AudioBus* buffer) {
  DCHECK(buffer);
  DCHECK_EQ(num_channels_, buffer->channels());
  DCHECK_GE(buffer->frames(), num_frames);

  int64_t playback_absolute_timestamp = rendering_delay.delay_microseconds +
                                        rendering_delay.timestamp_microseconds;

  int filled = 0;
  bool queued_more_data = false;
  bool signal_eos = false;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);

    // Playback start check.
    if (!started_ &&
        (state_ == State::kNormalPlayback || state_ == State::kGotEos)) {
      CheckAndStartPlaybackIfNecessary(num_frames, playback_absolute_timestamp);
    }

    // In normal playback, don't pass data to the fader if we can't satisfy the
    // full request. This will allow us to buffer up more data so we can fully
    // fade in.
    if (state_ == State::kNormalPlayback && started_ &&
        queued_frames_ < fader_.FramesNeededFromSource(num_frames)) {
      LOG_IF(INFO, !zero_fader_frames_)
          << "Stream underrun for " << device_id_ << " (" << this << ")";
      zero_fader_frames_ = true;
    } else {
      LOG_IF(INFO, started_ && zero_fader_frames_)
          << "Stream underrun recovered for " << device_id_ << " (" << this
          << ")";
      zero_fader_frames_ = false;
    }

    DCHECK_GE(remaining_silence_frames_, 0);
    if (remaining_silence_frames_ >= num_frames) {
      remaining_silence_frames_ -= num_frames;
      return 0;
    }

    int write_offset = 0;
    if (remaining_silence_frames_ > 0) {
      buffer->ZeroFramesPartial(0, remaining_silence_frames_);
      filled += remaining_silence_frames_;
      num_frames -= remaining_silence_frames_;
      write_offset = remaining_silence_frames_;
      remaining_silence_frames_ = 0;
    }

    float* channels[num_channels_];
    for (int c = 0; c < num_channels_; ++c) {
      channels[c] = buffer->channel(c) + write_offset;
    }
    filled = fader_.FillFrames(num_frames, rendering_delay, channels);

    mixer_rendering_delay_ = rendering_delay;
    extra_delay_frames_ = num_frames + fader_.buffered_frames();

    // See if we can accept more data into the queue.
    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      last_buffer_delay_ = QueueData(std::move(pending_data_));
      queued_more_data = true;
    }

    // Check if we have played out EOS.
    if (state_ == State::kGotEos && queued_frames_ == 0 &&
        fader_.buffered_frames() == 0) {
      signal_eos = true;
      state_ = State::kSignaledEos;
    }

    // If the caller has removed this source, delete once we have faded out.
    if (state_ == State::kRemoved && fader_.buffered_frames() == 0) {
      remove_self = true;
    }
  }

  if (queued_more_data) {
    caller_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
  if (signal_eos) {
    caller_task_runner_->PostTask(FROM_HERE, eos_task_);
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
  return filled;
}

int64_t BufferingMixerSource::DataToFrames(int64_t size_in_bytes) {
  return size_in_bytes / (num_channels_ * sizeof(float));
}

int BufferingMixerSource::FillFaderFrames(int num_frames,
                                          RenderingDelay rendering_delay,
                                          float* const* channels) {
  DCHECK(channels);

  if (zero_fader_frames_ || !started_ || paused_ || state_ == State::kRemoved) {
    return 0;
  }

  int num_filled = 0;
  while (num_frames) {
    if (queue_.empty()) {
      return num_filled;
    }

    DecoderBufferBase* buffer = queue_.front().get();
    const int buffer_frames = DataToFrames(buffer->data_size());
    const int frames_to_copy =
        std::min(num_frames, buffer_frames - current_buffer_offset_);
    DCHECK(frames_to_copy >= 0 && frames_to_copy <= num_frames)
        << " frames_to_copy=" << frames_to_copy << " num_frames=" << num_frames
        << " buffer_frames=" << buffer_frames << " num_filled=" << num_filled
        << " current_buffer_offset_=" << current_buffer_offset_
        << " buffer=" << buffer->data_size();

    const float* buffer_samples =
        reinterpret_cast<const float*>(buffer->data());
    for (int c = 0; c < num_channels_; ++c) {
      const float* buffer_channel = buffer_samples + (buffer_frames * c);
      std::copy_n(buffer_channel + current_buffer_offset_, frames_to_copy,
                  channels[c] + num_filled);
    }

    num_frames -= frames_to_copy;
    queued_frames_ -= frames_to_copy;
    num_filled += frames_to_copy;

    current_buffer_offset_ += frames_to_copy;
    if (current_buffer_offset_ == buffer_frames) {
      buffers_to_be_freed_.push_back(std::move(queue_.front()));
      queue_.pop_front();
      current_buffer_offset_ = 0;
    }
  }

  return num_filled;
}

void BufferingMixerSource::PostPcmCompletion() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  RenderingDelay delay;
  {
    base::AutoLock lock(lock_);
    delay = last_buffer_delay_;
  }
  delegate_->OnWritePcmCompletion(delay);
}

void BufferingMixerSource::PostEos() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_->OnEos();
}

void BufferingMixerSource::PostAudioReadyForPlayback() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_);
  if (!audio_ready_for_playback_fired_) {
    audio_ready_for_playback_fired_ = true;
    delegate_->OnAudioReadyForPlayback();
  }
}

void BufferingMixerSource::OnAudioPlaybackError(MixerError error) {
  if (error == MixerError::kInputIgnored) {
    LOG(INFO) << "Mixer input " << device_id_ << " (" << this << ")"
              << " now being ignored due to output sample rate change";
  }

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BufferingMixerSource::PostError, weak_this_, error));

  base::AutoLock lock(lock_);
  mixer_error_ = true;
  if (state_ == State::kRemoved) {
    mixer_->RemoveInput(this);
  }
}

void BufferingMixerSource::PostError(MixerError error) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_->OnMixerError(error);
}

void BufferingMixerSource::Remove() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();

  LOG(INFO) << "Remove " << device_id_ << " (" << this << ")";
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);
    pending_data_ = nullptr;
    state_ = State::kRemoved;
    remove_self = mixer_error_;
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
}

void BufferingMixerSource::FinalizeAudioPlayback() {
  delete this;
}

}  // namespace media
}  // namespace chromecast
