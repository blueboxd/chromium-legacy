// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer_service_receiver.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/cma/backend/mixer_input_connection.h"

namespace chromecast {
namespace media {

MixerServiceReceiver::MixerServiceReceiver(StreamMixer* mixer) : mixer_(mixer) {
  DCHECK(mixer_);
}

MixerServiceReceiver::~MixerServiceReceiver() = default;

void MixerServiceReceiver::CreateOutputStream(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  DCHECK(message.has_output_stream_params());
  // MixerInputConnection manages its own lifetime.
  auto* connection = new MixerInputConnection(mixer_, std::move(socket),
                                              message.output_stream_params());
  connection->HandleMetadata(message);
}

void MixerServiceReceiver::CreateLoopbackConnection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  LOG(INFO) << "Unhandled loopback connection";
}

void MixerServiceReceiver::CreateAudioRedirection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  LOG(INFO) << "Unhandled redirection connection";
}

void MixerServiceReceiver::CreateControlConnection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  LOG(INFO) << "Unhandled control connection";
}

}  // namespace media
}  // namespace chromecast
