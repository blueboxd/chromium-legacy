// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/cast_streaming_session_client.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/media_util.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

CastStreamingSessionClient::CastStreamingSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request)
    : message_port_request_(std::move(message_port_request)) {}

CastStreamingSessionClient::~CastStreamingSessionClient() = default;

void CastStreamingSessionClient::StartMojoConnection(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver) {
  DVLOG(1) << __func__;
  cast_streaming_receiver_ = std::move(cast_streaming_receiver);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedRemote, is owned by |this| and will be torn-down at the same time
  // as |this|.
  cast_streaming_receiver_->EnableReceiver(base::BindOnce(
      &CastStreamingSessionClient::OnReceiverEnabled, base::Unretained(this)));
  cast_streaming_receiver_.set_disconnect_handler(base::BindOnce(
      &CastStreamingSessionClient::OnMojoDisconnect, base::Unretained(this)));
}

void CastStreamingSessionClient::OnReceiverEnabled() {
  DVLOG(1) << __func__;
  DCHECK(message_port_request_);
  cast_streaming_session_.Start(this, std::move(message_port_request_),
                                base::SequencedTaskRunnerHandle::Get());
}

void CastStreamingSessionClient::OnInitializationSuccess(
    base::Optional<media::AudioDecoderConfig> audio_decoder_config,
    base::Optional<media::VideoDecoderConfig> video_decoder_config) {
  DVLOG(1) << __func__;
  DCHECK(audio_decoder_config || video_decoder_config);

  mojom::AudioStreamInfoPtr audio_stream_info;
  if (audio_decoder_config) {
    mojo::ScopedDataPipeConsumerHandle audio_consumer;
    audio_buffer_writer_ = media::MojoDecoderBufferWriter::Create(
        media::GetDefaultDecoderBufferConverterCapacity(
            media::DemuxerStream::Type::AUDIO),
        &audio_consumer);
    audio_stream_info = mojom::AudioStreamInfo::New(
        audio_decoder_config.value(),
        audio_remote_.BindNewPipeAndPassReceiver(), std::move(audio_consumer));
  }

  mojom::VideoStreamInfoPtr video_stream_info;
  if (video_decoder_config) {
    mojo::ScopedDataPipeConsumerHandle video_consumer;
    video_buffer_writer_ = media::MojoDecoderBufferWriter::Create(
        media::GetDefaultDecoderBufferConverterCapacity(
            media::DemuxerStream::Type::VIDEO),
        &video_consumer);
    video_stream_info = mojom::VideoStreamInfo::New(
        video_decoder_config.value(),
        video_remote_.BindNewPipeAndPassReceiver(), std::move(video_consumer));
  }

  cast_streaming_receiver_->OnStreamsInitialized(std::move(audio_stream_info),
                                                 std::move(video_stream_info));
}

void CastStreamingSessionClient::OnInitializationFailure() {
  DVLOG(1) << __func__;
  cast_streaming_receiver_.reset();
}

void CastStreamingSessionClient::OnAudioFrameReceived(
    scoped_refptr<media::DecoderBuffer> buffer) {
  DVLOG(3) << __func__;
  DCHECK(audio_buffer_writer_);
  media::mojom::DecoderBufferPtr mojo_decoder_buffer =
      audio_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_decoder_buffer) {
    DVLOG(2) << "Audio mojo pipe has been closed.";
    return;
  }
  audio_remote_->ProvideBuffer(std::move(mojo_decoder_buffer));
}

void CastStreamingSessionClient::OnVideoFrameReceived(
    scoped_refptr<media::DecoderBuffer> buffer) {
  DVLOG(3) << __func__;
  DCHECK(video_buffer_writer_);
  media::mojom::DecoderBufferPtr mojo_decoder_buffer =
      video_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_decoder_buffer) {
    DVLOG(2) << "Video mojo pipe has been closed.";
    return;
  }
  video_remote_->ProvideBuffer(std::move(mojo_decoder_buffer));
}

void CastStreamingSessionClient::OnReceiverSessionEnded() {
  DVLOG(1) << __func__;

  // Tear down the Mojo connection.
  cast_streaming_receiver_.reset();
}

void CastStreamingSessionClient::OnMojoDisconnect() {
  DVLOG(1) << __func__;

  if (message_port_request_) {
    // Close the MessagePort if the Cast Streaming Session was never started.
    message_port_request_.Close(ZX_ERR_PEER_CLOSED);
    cast_streaming_receiver_.reset();
    return;
  }

  // Close the Cast Streaming Session. This will eventually call
  // OnReceiverSessionEnded(), which will tear down the Mojo connection.
  cast_streaming_session_.Stop();

  // Tear down all remaining Mojo objects.
  audio_buffer_writer_.reset();
  video_buffer_writer_.reset();
  audio_remote_.reset();
  video_remote_.reset();
}
