// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/audio_capturer_redirect.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/fuchsia/default_context.h"

AudioCapturerRedirect::AudioCapturerRedirect(
    sys::OutgoingDirectory* outgoing_directory,
    CreateCapturerCallback create_capturer_callback)
    : binding_(outgoing_directory, this),
      create_capturer_callback_(std::move(create_capturer_callback)),
      system_audio_(base::fuchsia::ComponentContextForCurrentProcess()
                        ->svc()
                        ->Connect<fuchsia::media::Audio>()) {}
AudioCapturerRedirect::~AudioCapturerRedirect() = default;

void AudioCapturerRedirect::CreateAudioRenderer(
    fidl::InterfaceRequest<fuchsia::media::AudioRenderer>
        audio_renderer_request) {
  system_audio_->CreateAudioRenderer(std::move(audio_renderer_request));
}

void AudioCapturerRedirect::CreateAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer>
        audio_capturer_request,
    bool loopback) {
  // Loopback capture is not supported.
  if (loopback) {
    NOTREACHED();
    return;
  }

  create_capturer_callback_.Run(std::move(audio_capturer_request));
}

void AudioCapturerRedirect::SetSystemMute(bool muted) {
  system_audio_->SetSystemMute(muted);
}

void AudioCapturerRedirect::SetSystemGain(float gain_db) {
  system_audio_->SetSystemGain(gain_db);
}
