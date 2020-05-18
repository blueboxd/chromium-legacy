// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/cast_streaming_session_client.h"

#include "base/threading/sequenced_task_runner_handle.h"

CastStreamingSessionClient::CastStreamingSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request) {
  // TODO(crbug.com/1042501): Start the session on-demand from the renderer.
  cast_streaming_session_.Start(this, std::move(message_port_request),
                                base::SequencedTaskRunnerHandle::Get());
}

CastStreamingSessionClient::~CastStreamingSessionClient() = default;

void CastStreamingSessionClient::OnInitializationSuccess(
    base::Optional<media::AudioDecoderConfig> audio_decoder_config,
    base::Optional<media::VideoDecoderConfig> video_decoder_config) {
  // TODO(crbug.com/1042501): Initialize the Demuxer in the renderer process.
}

void CastStreamingSessionClient::OnInitializationFailure() {
  // TODO(crbug.com/1042501): Cancel initialization.
}

void CastStreamingSessionClient::OnAudioFrameReceived(
    scoped_refptr<media::DecoderBuffer> buffer) {
  // TODO(crbug.com/1042501): Send the frame to the renderer process.
}

void CastStreamingSessionClient::OnVideoFrameReceived(
    scoped_refptr<media::DecoderBuffer> buffer) {
  // TODO(crbug.com/1042501): Send the frame to the renderer process.
}

void CastStreamingSessionClient::OnReceiverSessionEnded() {
  // TODO(crbug.com/1042501): End the Mojo service.
}
