// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"
#include "chromecast/public/volume_control.h"

#define RUN_ON_MEDIA_THREAD(method, ...)                              \
  media_task_runner_->PostTask(                                       \
      FROM_HERE, base::BindOnce(&MediaPipelineBackendManager::method, \
                                weak_factory_.GetWeakPtr(), ##__VA_ARGS__));

#define MAKE_SURE_MEDIA_THREAD(method, ...)            \
  if (!media_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_MEDIA_THREAD(method, ##__VA_ARGS__)         \
    return;                                            \
  }

namespace chromecast {
namespace media {

namespace {

constexpr int kAudioDecoderLimit = std::numeric_limits<int>::max();
constexpr int kVideoDecoderLimit = 1;
constexpr base::TimeDelta kPowerSaveWaitTime = base::TimeDelta::FromSeconds(5);

}  // namespace

MediaPipelineBackendManager::MediaPipelineBackendManager(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)),
      playing_audio_streams_count_({{AudioContentType::kMedia, 0},
                                    {AudioContentType::kAlarm, 0},
                                    {AudioContentType::kCommunication, 0},
                                    {AudioContentType::kOther, 0}}),
      playing_noneffects_audio_streams_count_(
          {{AudioContentType::kMedia, 0},
           {AudioContentType::kAlarm, 0},
           {AudioContentType::kCommunication, 0},
           {AudioContentType::kOther, 0}}),
      allow_volume_feedback_observers_(
          new base::ObserverListThreadSafe<AllowVolumeFeedbackObserver>()),
      backend_wrapper_using_video_decoder_(nullptr),
      buffer_delegate_(nullptr),
      weak_factory_(this) {
  DCHECK(media_task_runner_);
  DCHECK(playing_audio_streams_count_.size() ==
         static_cast<unsigned long>(AudioContentType::kNumTypes));
  DCHECK(playing_noneffects_audio_streams_count_.size() ==
         static_cast<unsigned long>(AudioContentType::kNumTypes));
  for (int i = 0; i < NUM_DECODER_TYPES; ++i) {
    decoder_count_[i] = 0;
  }
}

MediaPipelineBackendManager::~MediaPipelineBackendManager() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
}

std::unique_ptr<CmaBackend> MediaPipelineBackendManager::CreateCmaBackend(
    const media::MediaPipelineDeviceParams& params) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return std::make_unique<MediaPipelineBackendWrapper>(params, this);
}

void MediaPipelineBackendManager::BackendDestroyed(
    MediaPipelineBackendWrapper* backend_wrapper) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (backend_wrapper_using_video_decoder_ == backend_wrapper) {
    backend_wrapper_using_video_decoder_ = nullptr;
  }
}

void MediaPipelineBackendManager::BackendUseVideoDecoder(
    MediaPipelineBackendWrapper* backend_wrapper) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(backend_wrapper);
  if (backend_wrapper_using_video_decoder_ &&
      backend_wrapper_using_video_decoder_ != backend_wrapper) {
    LOG(INFO) << __func__ << " revoke old backend : "
              << backend_wrapper_using_video_decoder_;
    backend_wrapper_using_video_decoder_->Revoke();
  }
  backend_wrapper_using_video_decoder_ = backend_wrapper;
}

bool MediaPipelineBackendManager::IncrementDecoderCount(DecoderType type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(type < NUM_DECODER_TYPES);
  const int limit =
      (type == VIDEO_DECODER) ? kVideoDecoderLimit : kAudioDecoderLimit;
  if (decoder_count_[type] >= limit) {
    LOG(WARNING) << "Decoder limit reached for type " << type;
    return false;
  }

  ++decoder_count_[type];
  return true;
}

void MediaPipelineBackendManager::DecrementDecoderCount(DecoderType type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(type < NUM_DECODER_TYPES);
  DCHECK_GT(decoder_count_[type], 0);

  decoder_count_[type]--;
}

void MediaPipelineBackendManager::UpdatePlayingAudioCount(
    bool sfx,
    const AudioContentType type,
    int change) {
  DCHECK(change == -1 || change == 1) << "bad count change: " << change;

  bool had_playing_audio_streams = (TotalPlayingAudioStreamsCount() > 0);
  playing_audio_streams_count_[type] += change;
  DCHECK_GE(playing_audio_streams_count_[type], 0);

  int new_playing_audio_streams = TotalPlayingAudioStreamsCount();
  if (new_playing_audio_streams == 0) {
    power_save_timer_.Start(FROM_HERE, kPowerSaveWaitTime, this,
                            &MediaPipelineBackendManager::EnterPowerSaveMode);
  } else if (!had_playing_audio_streams && new_playing_audio_streams > 0) {
    power_save_timer_.Stop();
    if (VolumeControl::SetPowerSaveMode) {
      metrics::CastMetricsHelper::GetInstance()->RecordSimpleAction(
          "Cast.Platform.VolumeControl.PowerSaveOff");
      VolumeControl::SetPowerSaveMode(false);
    }
  }

  if (sfx) {
    return;
  }

  // Volume feedback sounds are only allowed when there are no non-effects
  // audio streams playing.
  bool prev_allow_feedback = (TotalPlayingNoneffectsAudioStreamsCount() == 0);
  playing_noneffects_audio_streams_count_[type] += change;
  DCHECK_GE(playing_noneffects_audio_streams_count_[type], 0);
  bool new_allow_feedback = (TotalPlayingNoneffectsAudioStreamsCount() == 0);

  if (new_allow_feedback != prev_allow_feedback) {
    allow_volume_feedback_observers_->Notify(
        FROM_HERE, &AllowVolumeFeedbackObserver::AllowVolumeFeedbackSounds,
        new_allow_feedback);
  }
}

int MediaPipelineBackendManager::TotalPlayingAudioStreamsCount() {
  int total = 0;
  for (auto entry : playing_audio_streams_count_) {
    total += entry.second;
  }
  return total;
}

int MediaPipelineBackendManager::TotalPlayingNoneffectsAudioStreamsCount() {
  int total = 0;
  for (auto entry : playing_noneffects_audio_streams_count_) {
    total += entry.second;
  }
  return total;
}

void MediaPipelineBackendManager::EnterPowerSaveMode() {
  DCHECK_EQ(TotalPlayingAudioStreamsCount(), 0);
  if (!VolumeControl::SetPowerSaveMode || !power_save_enabled_) {
    return;
  }
  metrics::CastMetricsHelper::GetInstance()->RecordSimpleAction(
      "Cast.Platform.VolumeControl.PowerSaveOn");
  VolumeControl::SetPowerSaveMode(true);
}

void MediaPipelineBackendManager::AddAllowVolumeFeedbackObserver(
    AllowVolumeFeedbackObserver* observer) {
  allow_volume_feedback_observers_->AddObserver(observer);
}

void MediaPipelineBackendManager::RemoveAllowVolumeFeedbackObserver(
    AllowVolumeFeedbackObserver* observer) {
  allow_volume_feedback_observers_->RemoveObserver(observer);
}

void MediaPipelineBackendManager::AddExtraPlayingStream(
    bool sfx,
    const AudioContentType type) {
  MAKE_SURE_MEDIA_THREAD(AddExtraPlayingStream, sfx, type);
  UpdatePlayingAudioCount(sfx, type, 1);
}

void MediaPipelineBackendManager::RemoveExtraPlayingStream(
    bool sfx,
    const AudioContentType type) {
  MAKE_SURE_MEDIA_THREAD(RemoveExtraPlayingStream, sfx, type);
  UpdatePlayingAudioCount(sfx, type, -1);
}

void MediaPipelineBackendManager::SetBufferDelegate(
    BufferDelegate* buffer_delegate) {
  MAKE_SURE_MEDIA_THREAD(SetBufferDelegate, buffer_delegate);
  DCHECK(buffer_delegate);
  DCHECK(!buffer_delegate_);
  buffer_delegate_ = buffer_delegate;
}

bool MediaPipelineBackendManager::IsPlaying(bool include_sfx,
                                            AudioContentType type) {
  if (include_sfx) {
    return playing_audio_streams_count_[type];
  } else {
    return playing_noneffects_audio_streams_count_[type];
  }
}

void MediaPipelineBackendManager::SetPowerSaveEnabled(bool power_save_enabled) {
  MAKE_SURE_MEDIA_THREAD(SetPowerSaveEnabled, power_save_enabled);
  power_save_enabled_ = power_save_enabled;
  if (VolumeControl::SetPowerSaveMode && !power_save_enabled_) {
    VolumeControl::SetPowerSaveMode(false);
  }
}

}  // namespace media
}  // namespace chromecast
