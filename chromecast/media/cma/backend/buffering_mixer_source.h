// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_BUFFERING_MIXER_SOURCE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_BUFFERING_MIXER_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chromecast/media/cma/backend/audio_fader.h"
#include "chromecast/media/cma/backend/audio_resampler.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {
class DecoderBufferBase;
class StreamMixer;

// A mixer source that buffers some amount of data to smooth over any delays in
// the stream. Instances of this class manage their own lifetime; therefore the
// destructor is private. Methods may be called on any thread; delegate methods
// will be called on the thread that created the BufferingMixerSource instance.
class BufferingMixerSource : public MixerInput::Source,
                             public AudioFader::Source {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  class Delegate {
   public:
    using MixerError = MixerInput::Source::MixerError;

    // Called when the last data passed to WritePcm() has been successfully
    // added to the buffer. |delay| (if valid) indicates the expected playout
    // time of the next pushed buffer.
    virtual void OnWritePcmCompletion(RenderingDelay delay) = 0;

    // Called when a mixer error occurs.
    virtual void OnMixerError(MixerError error) = 0;

    // Called when the end-of-stream buffer has been played out.
    virtual void OnEos() = 0;

    // Called when the audio is ready to play.
    virtual void OnAudioReadyForPlayback() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Can be used as a custom deleter for unique_ptr.
  struct Deleter {
    void operator()(BufferingMixerSource* obj) { obj->Remove(); }
  };

  BufferingMixerSource(Delegate* delegate,
                       int input_samples_per_second,
                       bool primary,
                       const std::string& device_id,
                       AudioContentType content_type,
                       int playout_channel,
                       int64_t playback_start_pts,
                       bool start_playback_asap);

  // Specifies the absolute timestamp (relative to the RenderingDelay clock)
  // that the playback should start at. This should only be called if
  // |start_playback_asap| is false during constructing.
  void StartPlaybackAt(int64_t playback_start_timestamp);

  // Restarts the current playback from the timestamp provided at the pts
  // provided. Flushes any currently buffered audio. Generally does well if you
  // require the audio to jump back and/or forth by up to 5 seconds or so,
  // depending on how much data is already buffered by the upper layers and
  // ready for consumption here. This API will start having problems if you try
  // to do more than that, so it's not advised.
  void RestartPlaybackAt(int64_t timestamp, int64_t pts);

  // Queues some PCM data to be mixed. |data| must be in planar float format.
  // If the buffer can accept more data, the delegate's OnWritePcmCompletion()
  // method is called synchronously. Otherwise, OnWritePcmCompletion() will be
  // called asynchronously once the buffer is able to accept more data.
  void WritePcm(scoped_refptr<DecoderBufferBase> data);

  // Sets the pause state of this stream.
  void SetPaused(bool paused);

  // Sets the volume multiplier for this stream. If |multiplier| < 0, sets the
  // volume multiplier to 0.
  void SetVolumeMultiplier(float multiplier);

  // Removes this source from the mixer asynchronously. After this method is
  // called, no more calls will be made to delegate methods. The source will
  // be removed from the mixer once it has faded out appropriately.
  void Remove();

  // Sets the current media playback rate (typically ranges between 0.5 - 2.0).
  void SetMediaPlaybackRate(double rate);

  // This allows for very small changes in the rate of audio playback that are
  // (supposedly) imperceptible.
  float SetAvSyncPlaybackRate(float rate);

  // Returns the rendering delay from the mixer (ie, ignores any buffering in
  // this class).
  RenderingDelay GetMixerRenderingDelay();

 private:
  enum class State {
    kUninitialized,   // Not initialized by the mixer yet.
    kNormalPlayback,  // Normal playback.
    kGotEos,          // Got the end-of-stream buffer (normal playback).
    kSignaledEos,     // Sent EOS signal up to delegate.
    kRemoved,         // The caller has removed this source; finish playing out.
  };

  // Class to wrap members that must be used only when a lock is held. Usage:
  // {
  //   auto locked = locked_members_.Lock();
  //   locked->paused_ = true;  // or other member access
  // }
  class LockedMembers {
   public:
    struct Members {
      Members(BufferingMixerSource* source,
              int input_samples_per_second,
              int num_channels,
              int64_t playback_start_timestamp,
              int64_t playback_start_pts);
      ~Members();

      State state_ = State::kUninitialized;
      bool paused_ = false;
      bool mixer_error_ = false;
      scoped_refptr<DecoderBufferBase> pending_data_;
      base::circular_deque<scoped_refptr<DecoderBufferBase>> queue_;
      // We let the caller thread free audio buffers since freeing memory can
      // be expensive sometimes; we want to avoid potentially long-running
      // operations on the mixer thread.
      std::vector<scoped_refptr<DecoderBufferBase>> buffers_to_be_freed_;
      int queued_frames_ = 0;
      RenderingDelay mixer_rendering_delay_;
      RenderingDelay last_buffer_delay_;
      int extra_delay_frames_ = 0;
      int current_buffer_offset_ = 0;
      AudioFader fader_;
      bool zero_fader_frames_ = false;
      bool started_ = false;
      double playback_rate_ = 1.0;
      // The absolute timestamp relative to clock monotonic (raw) at which the
      // playback should start. INT64_MIN indicates playback should start ASAP.
      // INT64_MAX indicates playback should start at a specified timestamp,
      // but we don't know what that timestamp is.
      int64_t playback_start_timestamp_ = INT64_MIN;
      // The PTS the playback should start at. We will drop audio pushed to us
      // with PTS values below this value. If the audio doesn't have a starting
      // PTS, then this value can be INT64_MIN, to play whatever audio is sent
      // to us.
      int64_t playback_start_pts_ = INT64_MIN;
      AudioResampler audio_resampler_;

     private:
      DISALLOW_COPY_AND_ASSIGN(Members);
    };

    class AcquiredLock {
     public:
      explicit AcquiredLock(LockedMembers* locked);
      ~AcquiredLock();

      Members* operator->() { return &(locked_->members_); }

     private:
      LockedMembers* const locked_;
    };

    class AssertedLock {
     public:
      explicit AssertedLock(LockedMembers* locked);

      Members* operator->() { return &(locked_->members_); }

     private:
      LockedMembers* const locked_;
    };

    LockedMembers(BufferingMixerSource* source,
                  int input_samples_per_second,
                  int num_channels,
                  int64_t playback_start_timestamp,
                  int64_t playback_start_pts);
    ~LockedMembers();

    AcquiredLock Lock();
    AssertedLock AssertAcquired();

   private:
    base::Lock member_lock_;
    Members members_;

    DISALLOW_COPY_AND_ASSIGN(LockedMembers);
  };

  ~BufferingMixerSource() override;

  // MixerInput::Source implementation:
  int num_channels() override;
  int input_samples_per_second() override;
  bool primary() override;
  const std::string& device_id() override;
  AudioContentType content_type() override;
  int desired_read_size() override;
  int playout_channel() override;

  void InitializeAudioPlayback(int read_size,
                               RenderingDelay initial_rendering_delay) override;
  int FillAudioPlaybackFrames(int num_frames,
                              RenderingDelay rendering_delay,
                              ::media::AudioBus* buffer) override;
  void OnAudioPlaybackError(MixerError error) override;
  void FinalizeAudioPlayback() override;

  // AudioFader::Source implementation:
  int FillFaderFrames(int num_frames,
                      RenderingDelay rendering_delay,
                      float* const* channels) override;

  RenderingDelay QueueData(scoped_refptr<DecoderBufferBase> data);

  void PostPcmCompletion();
  void PostEos();
  void PostError(MixerError error);
  void PostAudioReadyForPlayback();
  void DropAudio(int64_t frames);
  int64_t DataToFrames(int64_t size);
  void CheckAndStartPlaybackIfNecessary(int num_frames,
                                        int64_t playback_absolute_timestamp);
  Delegate* const delegate_;
  const int num_channels_;
  const int input_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;
  const int playout_channel_;
  StreamMixer* const mixer_;
  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  const int max_queued_frames_;
  // Minimum number of frames buffered before starting to fill data.
  const int start_threshold_frames_;
  bool audio_ready_for_playback_fired_ = false;

  // Only used on the caller thread.
  std::vector<scoped_refptr<DecoderBufferBase>> old_buffers_to_be_freed_;

  LockedMembers locked_members_;

  int remaining_silence_frames_ = 0;

  base::RepeatingClosure pcm_completion_task_;
  base::RepeatingClosure eos_task_;
  base::RepeatingClosure ready_for_playback_task_;

  base::WeakPtr<BufferingMixerSource> weak_this_;
  base::WeakPtrFactory<BufferingMixerSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BufferingMixerSource);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_BUFFERING_MIXER_SOURCE_H_
