// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_AUDIO_CAPTURER_REDIRECT_H_
#define FUCHSIA_RUNNERS_CAST_AUDIO_CAPTURER_REDIRECT_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/callback.h"
#include "base/fuchsia/scoped_service_binding.h"

// fuchsia::media::Audio implementation that redirects CreateAudioCapturer()
// calls to a callback.
// TODO(fxb/47249) Remove this once AudioCapturerFactory is defined and
// implemented.
class AudioCapturerRedirect : public fuchsia::media::Audio {
 public:
  // Callback to be called for CreateAudioRenderer().
  using CreateCapturerCallback = base::RepeatingCallback<void(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request)>;

  // Publishes fuchsia.media.Audio to |outgoing_directory| The specified
  // |create_capturer_callback|. will be called every time CreateAudioCapturer()
  // is called. All other calls are redirected to /svc/fuchsia.media.Audio.
  AudioCapturerRedirect(sys::OutgoingDirectory* outgoing_directory,
                        CreateCapturerCallback create_capturer_callback);
  ~AudioCapturerRedirect() final;

 private:
  // fuchsia::media::Audio implementation.
  void CreateAudioRenderer(fidl::InterfaceRequest<fuchsia::media::AudioRenderer>
                               audio_renderer_request) final;
  void CreateAudioCapturer(fidl::InterfaceRequest<fuchsia::media::AudioCapturer>
                               audio_capturer_request,
                           bool loopback) final;
  void SetSystemMute(bool muted) final;
  void SetSystemGain(float gain_db) final;

  base::fuchsia::ScopedServiceBinding<fuchsia::media::Audio> binding_;
  CreateCapturerCallback create_capturer_callback_;
  fuchsia::media::AudioPtr system_audio_;
};

#endif  // FUCHSIA_RUNNERS_CAST_AUDIO_CAPTURER_REDIRECT_H_
