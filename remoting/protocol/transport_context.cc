// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_context.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/remoting_ice_config_request.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {
namespace protocol {

namespace {

// Ensure ICE config is correct at least one hour after session starts.
constexpr base::TimeDelta kMinimumIceConfigLifetime =
    base::TimeDelta::FromHours(1);

void PrintIceConfig(const IceConfig& ice_config) {
  HOST_LOG << "IceConfig: {";
  HOST_LOG << "  stun: [";
  for (auto& stun_server : ice_config.stun_servers) {
    HOST_LOG << "    " << stun_server.ToString() << ",";
  }
  HOST_LOG << "  ]";
  HOST_LOG << "  turn: [";
  for (auto& turn_server : ice_config.turn_servers) {
    HOST_LOG << "    {";
    HOST_LOG << "      username: " << turn_server.credentials.username;
    HOST_LOG << "      password: " << turn_server.credentials.password;
    for (auto& port : turn_server.ports) {
      HOST_LOG << "      port: " << port.address.ToString();
    }
    HOST_LOG << "    },";
  }
  HOST_LOG << "  ]";
  HOST_LOG << "  expiration time: " << ice_config.expiration_time;
  HOST_LOG << "  max_bitrate_kbps: " << ice_config.max_bitrate_kbps;
  HOST_LOG << "}";
}

}  // namespace

// static
scoped_refptr<TransportContext> TransportContext::ForTests(TransportRole role) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  return new protocol::TransportContext(
      std::make_unique<protocol::ChromiumPortAllocatorFactory>(), nullptr,
      protocol::NetworkSettings(
          protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING),
      role);
}

TransportContext::TransportContext(
    std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
    std::unique_ptr<UrlRequestFactory> url_request_factory,
    const NetworkSettings& network_settings,
    TransportRole role)
    : port_allocator_factory_(std::move(port_allocator_factory)),
      url_request_factory_(std::move(url_request_factory)),
      network_settings_(network_settings),
      role_(role) {}

TransportContext::~TransportContext() = default;

void TransportContext::Prepare() {
  EnsureFreshIceConfig();
}

void TransportContext::GetIceConfig(const GetIceConfigCallback& callback) {
  EnsureFreshIceConfig();

  // If there is a pending |ice_config_request_| then delay the callback until
  // the request is finished.
  if (ice_config_request_) {
    pending_ice_config_callbacks_.push_back(callback);
  } else {
    HOST_LOG << "Using cached ICE Config.";
    PrintIceConfig(ice_config_);
    callback.Run(ice_config_);
  }
}

void TransportContext::EnsureFreshIceConfig() {
  // Check if request is already pending.
  if (ice_config_request_) {
    HOST_LOG << "ICE Config request is already pending.";
    return;
  }

  // Don't need to make ICE config request if both STUN and Relay are disabled.
  if ((network_settings_.flags & (NetworkSettings::NAT_TRAVERSAL_STUN |
                                  NetworkSettings::NAT_TRAVERSAL_RELAY)) == 0) {
    HOST_LOG << "Skipping ICE Config request as STUN and RELAY are disabled";
    return;
  }

  if ((base::Time::Now() + kMinimumIceConfigLifetime) >
      ice_config_.expiration_time) {
    ice_config_request_ = std::make_unique<RemotingIceConfigRequest>();
    ice_config_request_->Send(
        base::BindOnce(&TransportContext::OnIceConfig, base::Unretained(this)));
  }
}

void TransportContext::OnIceConfig(const IceConfig& ice_config) {
  ice_config_ = ice_config;
  ice_config_request_.reset();

  HOST_LOG << "Using newly requested ICE Config:";
  PrintIceConfig(ice_config);

  auto& callback_list = pending_ice_config_callbacks_;
  while (!callback_list.empty()) {
    callback_list.begin()->Run(ice_config);
    callback_list.pop_front();
  }
}

int TransportContext::GetTurnMaxRateKbps() const {
  return ice_config_.max_bitrate_kbps;
}

}  // namespace protocol
}  // namespace remoting
