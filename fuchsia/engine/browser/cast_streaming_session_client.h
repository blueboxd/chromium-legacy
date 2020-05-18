// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia/cast_streaming/public/cast_streaming_session.h"

// Owns the CastStreamingSession and sends frames to the renderer process via
// a Mojo service.
// TODO(crbug.com/1042501): Connect this to a mojo service to send frames to the
// renderer process.
class CastStreamingSessionClient
    : public cast_streaming::CastStreamingSession::Client {
 public:
  explicit CastStreamingSessionClient(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request);
  ~CastStreamingSessionClient() final;

  CastStreamingSessionClient(const CastStreamingSessionClient&) = delete;
  CastStreamingSessionClient& operator=(const CastStreamingSessionClient&) =
      delete;

 private:
  // cast_streaming::CastStreamingSession::Client implementation.
  void OnInitializationSuccess(
      base::Optional<media::AudioDecoderConfig> audio_decoder_config,
      base::Optional<media::VideoDecoderConfig> video_decoder_config) final;
  void OnInitializationFailure() final;
  void OnAudioFrameReceived(scoped_refptr<media::DecoderBuffer> buffer) final;
  void OnVideoFrameReceived(scoped_refptr<media::DecoderBuffer> buffer) final;
  void OnReceiverSessionEnded() final;

  cast_streaming::CastStreamingSession cast_streaming_session_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_
