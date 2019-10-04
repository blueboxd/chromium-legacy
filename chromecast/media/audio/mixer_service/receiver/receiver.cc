// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/receiver/receiver.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {
namespace {

constexpr int kMaxAcceptLoop = 5;

std::string GetEndpoint() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string path =
      command_line->GetSwitchValueASCII(switches::kMixerServiceEndpoint);
  if (path.empty()) {
    return mixer_service::kDefaultUnixDomainSocketPath;
  }
  return path;
}

}  // namespace

class Receiver::InitialSocket : public MixerSocket::Delegate {
 public:
  InitialSocket(Receiver* receiver, std::unique_ptr<net::StreamSocket> socket)
      : receiver_(receiver),
        socket_(std::make_unique<MixerSocket>(std::move(socket), this)) {
    DCHECK(receiver_);
    socket_->ReceiveMessages();
  }

  ~InitialSocket() override = default;

 private:
  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override {
    if (message.has_output_stream_params()) {
      receiver_->CreateOutputStream(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_loopback_request()) {
      receiver_->CreateLoopbackConnection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_redirection_request()) {
      receiver_->CreateAudioRedirection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_set_device_volume() ||
               message.has_set_device_muted() ||
               message.has_set_volume_limit() ||
               message.has_configure_postprocessor() ||
               message.has_reload_postprocessors()) {
      receiver_->CreateControlConnection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    }

    return true;
  }

  void OnConnectionError() override { receiver_->RemoveInitialSocket(this); }

  Receiver* const receiver_;
  std::unique_ptr<MixerSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(InitialSocket);
};

Receiver::Receiver()
    : socket_service_(
          GetEndpoint(),
          GetSwitchValueNonNegativeInt(switches::kMixerServiceEndpoint,
                                       mixer_service::kDefaultTcpPort),
          kMaxAcceptLoop,
          this) {
  socket_service_.Accept();
}

Receiver::~Receiver() = default;

void Receiver::HandleAcceptedSocket(std::unique_ptr<net::StreamSocket> socket) {
  auto initial_socket =
      std::make_unique<InitialSocket>(this, std::move(socket));
  InitialSocket* ptr = initial_socket.get();
  initial_sockets_[ptr] = std::move(initial_socket);
}

void Receiver::RemoveInitialSocket(InitialSocket* socket) {
  initial_sockets_.erase(socket);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
