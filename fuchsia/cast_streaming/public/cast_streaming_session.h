// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_
#define FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_

#include <memory>

#include <fuchsia/web/cpp/fidl.h>

#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace cast_streaming {

// Entry point for the Cast Streaming Receiver implementation. Used to start a
// Cast Streaming Session for a provided FIDL MessagePort request.
class CastStreamingSession {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // Sets the NetworkContextGetter. This must be called before any call to
  // Start() and must only be called once. If the NetworkContext crashes, any
  // existing Cast Streaming Session will eventually terminate and call
  // OnReceiverSessionEnded().
  static void SetNetworkContextGetter(NetworkContextGetter getter);

  class Client {
   public:
    // Called when the Cast Streaming Session has been successfully initialized.
    // It is guaranteed that at least one of |audio_decoder_config| or
    // |video_decoder_config| will be set.
    virtual void OnInitializationSuccess(
        base::Optional<media::AudioDecoderConfig> audio_decoder_config,
        base::Optional<media::VideoDecoderConfig> video_decoder_config) = 0;

    // Called when the Cast Stream Session failed to initialize.
    virtual void OnInitializationFailure() = 0;

    // Called on every new audio frame after OnInitializationSuccess().
    virtual void OnAudioFrameReceived(
        scoped_refptr<media::DecoderBuffer> buffer) = 0;

    // Called on every new video frame after OnInitializationSuccess().
    virtual void OnVideoFrameReceived(
        scoped_refptr<media::DecoderBuffer> buffer) = 0;

    // Called when the Cast Streaming Session has ended.
    virtual void OnReceiverSessionEnded() = 0;

   protected:
    virtual ~Client();
  };

  CastStreamingSession();
  ~CastStreamingSession();

  CastStreamingSession(const CastStreamingSession&) = delete;
  CastStreamingSession& operator=(const CastStreamingSession&) = delete;

  // Starts the Cast Streaming Session. This can only be called once during the
  // lifespan of this object. |client| must not be null and must outlive this
  // object.
  // * On success, OnInitializationSuccess() will be called and
  //   OnAudioFrameReceived() and/or OnVideoFrameReceived() will be called on
  //   every subsequent Frame.
  // * On failure, OnInitializationFailure() will be called.
  void Start(
      Client* client,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  class Internal;
  std::unique_ptr<Internal> internal_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_
