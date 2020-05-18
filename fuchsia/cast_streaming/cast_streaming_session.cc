// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/public/cast_streaming_session.h"

#include <lib/zx/time.h>

#include "base/bind.h"
#include "base/notreached.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "components/openscreen_platform/task_runner.h"
#include "fuchsia/cast_streaming/cast_message_port_impl.h"
#include "fuchsia/cast_streaming/stream_consumer.h"
#include "media/base/media_util.h"
#include "third_party/openscreen/src/cast/streaming/receiver.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace {

// TODO(b/156117766): Remove these when Open Screen returns enum values rather
// than strings.
constexpr char kVideoCodecH264[] = "h264";
constexpr char kVideoCodecVp8[] = "vp8";

}  // namespace

namespace cast_streaming {

// static
void CastStreamingSession::SetNetworkContextGetter(
    NetworkContextGetter getter) {
  openscreen_platform::SetNetworkContextGetter(std::move(getter));
}

// Owns the Open Screen ReceiverSession. The Cast Streaming Session is tied to
// the lifespan of this object.
class CastStreamingSession::Internal
    : public openscreen::cast::ReceiverSession::Client {
 public:
  Internal(
      CastStreamingSession::Client* client,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner),
        environment_(&openscreen::Clock::now,
                     &task_runner_,
                     openscreen::IPEndpoint{
                         openscreen::IPAddress(0, 0, 0, 0, 0, 0, 0, 0), 0}),
        cast_message_port_impl_(std::move(message_port_request)),
        // TODO(crbug.com/1042501): Add streaming session Constraints and
        // DisplayDescription.
        receiver_session_(
            this,
            &environment_,
            &cast_message_port_impl_,
            openscreen::cast::ReceiverSession::Preferences(
                {openscreen::cast::ReceiverSession::VideoCodec::kH264,
                 openscreen::cast::ReceiverSession::VideoCodec::kVp8},
                {openscreen::cast::ReceiverSession::AudioCodec::kAac,
                 openscreen::cast::ReceiverSession::AudioCodec::kOpus})),
        client_(client) {
    DCHECK(task_runner);
    DCHECK(client_);
  }

  // TODO(b/156129407): Change to final when the base interface defines a
  // virtual destructor.
  virtual ~Internal() = default;

  Internal(const Internal&) = delete;
  Internal& operator=(const Internal&) = delete;

 private:
  // openscreen::cast::ReceiverSession::Client implementation.
  void OnNegotiated(
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) final {
    DVLOG(1) << __func__;
    DCHECK_EQ(session, &receiver_session_);

    base::Optional<media::AudioDecoderConfig> audio_decoder_config;
    if (receivers.audio) {
      audio_consumer_ = std::make_unique<StreamConsumer>(
          receivers.audio->receiver,
          base::BindRepeating(
              &CastStreamingSession::Client::OnAudioFrameReceived,
              base::Unretained(client_)));

      media::ChannelLayout channel_layout =
          media::GuessChannelLayout(receivers.audio->receiver_config.channels);
      const std::string& audio_codec =
          receivers.audio->selected_stream.stream.codec_name;
      media::AudioCodec media_audio_codec =
          media::StringToAudioCodec(audio_codec);
      int samples_per_second = receivers.audio->receiver_config.rtp_timebase;

      audio_decoder_config.emplace(media::AudioDecoderConfig(
          media_audio_codec, media::SampleFormat::kSampleFormatF32,
          channel_layout, samples_per_second, media::EmptyExtraData(),
          media::EncryptionScheme::kUnencrypted));

      DVLOG(1) << "Initialized audio stream using " << audio_codec << " codec.";
    }

    base::Optional<media::VideoDecoderConfig> video_decoder_config;
    if (receivers.video) {
      video_consumer_ = std::make_unique<StreamConsumer>(
          receivers.video->receiver,
          base::BindRepeating(
              &CastStreamingSession::Client::OnVideoFrameReceived,
              base::Unretained(client_)));

      const std::string& video_codec =
          receivers.video->selected_stream.stream.codec_name;
      uint32_t video_width =
          receivers.video->selected_stream.resolutions[0].width;
      uint32_t video_height =
          receivers.video->selected_stream.resolutions[0].height;
      gfx::Size video_size(video_width, video_height);
      gfx::Rect video_rect(video_width, video_height);

      if (video_codec == kVideoCodecH264) {
        video_decoder_config.emplace(media::VideoDecoderConfig(
            media::VideoCodec::kCodecH264,
            media::VideoCodecProfile::H264PROFILE_BASELINE,
            media::VideoDecoderConfig::AlphaMode::kIsOpaque,
            media::VideoColorSpace(), media::VideoTransformation(), video_size,
            video_rect, video_size, media::EmptyExtraData(),
            media::EncryptionScheme::kUnencrypted));
      } else if (video_codec == kVideoCodecVp8) {
        video_decoder_config.emplace(media::VideoDecoderConfig(
            media::VideoCodec::kCodecVP8,
            media::VideoCodecProfile::VP8PROFILE_MIN,
            media::VideoDecoderConfig::AlphaMode::kIsOpaque,
            media::VideoColorSpace(), media::VideoTransformation(), video_size,
            video_rect, video_size, media::EmptyExtraData(),
            media::EncryptionScheme::kUnencrypted));
      } else {
        NOTREACHED();
      }

      DVLOG(1) << "Initialized video stream of " << video_width << "x"
               << video_height << " resolution using " << video_codec
               << " codec.";
    }

    if (!video_decoder_config && !audio_decoder_config) {
      client_->OnInitializationFailure();
    } else {
      client_->OnInitializationSuccess(std::move(audio_decoder_config),
                                       std::move(video_decoder_config));
    }
    initialized_called_ = true;
  }

  void OnConfiguredReceiversDestroyed(
      const openscreen::cast::ReceiverSession* session) final {
    DCHECK_EQ(session, &receiver_session_);
    DVLOG(1) << __func__;
    audio_consumer_.reset();
    video_consumer_.reset();
    client_->OnReceiverSessionEnded();
  }

  void OnError(const openscreen::cast::ReceiverSession* session,
               openscreen::Error error) final {
    DCHECK_EQ(session, &receiver_session_);
    LOG(ERROR) << error;
    if (!initialized_called_) {
      client_->OnInitializationFailure();
      initialized_called_ = true;
    }
  }

  openscreen_platform::TaskRunner task_runner_;
  openscreen::cast::Environment environment_;
  CastMessagePortImpl cast_message_port_impl_;
  openscreen::cast::ReceiverSession receiver_session_;

  bool initialized_called_ = false;
  CastStreamingSession::Client* const client_;
  std::unique_ptr<openscreen::cast::Receiver::Consumer> audio_consumer_;
  std::unique_ptr<openscreen::cast::Receiver::Consumer> video_consumer_;
};

CastStreamingSession::Client::~Client() = default;
CastStreamingSession::CastStreamingSession() = default;
CastStreamingSession::~CastStreamingSession() = default;

void CastStreamingSession::Start(
    Client* client,
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(client);
  DCHECK(!internal_);
  internal_ = std::make_unique<Internal>(
      client, std::move(message_port_request), task_runner);
}

}  // namespace cast_streaming
