// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include "base/callback.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"
#include "jingle/glue/thread_wrapper.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace sharing {

SharingImpl::SharingImpl(mojo::PendingReceiver<mojom::Sharing> receiver)
    : receiver_(this, std::move(receiver)) {}

SharingImpl::~SharingImpl() = default;

void SharingImpl::CreateSharingWebRtcConnection(
    mojo::PendingRemote<mojom::SignallingSender> signalling_sender,
    mojo::PendingReceiver<mojom::SignallingReceiver> signalling_receiver,
    mojo::PendingRemote<mojom::SharingWebRtcConnectionDelegate> delegate,
    mojo::PendingReceiver<mojom::SharingWebRtcConnection> connection,
    mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
    mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
    std::vector<mojom::IceServerPtr> ice_servers) {
  if (!webrtc_peer_connection_factory_)
    InitializeWebRtcFactory();

  // base::Unretained is safe as the |peer_connection| is owned by |this|.
  auto sharing_connection = std::make_unique<SharingWebRtcConnection>(
      webrtc_peer_connection_factory_.get(), std::move(ice_servers),
      std::move(signalling_sender), std::move(signalling_receiver),
      std::move(delegate), std::move(connection), std::move(socket_manager),
      std::move(mdns_responder),
      base::BindOnce(&SharingImpl::SharingWebRtcConnectionDisconnected,
                     base::Unretained(this)));
  SharingWebRtcConnection* sharing_connection_ptr = sharing_connection.get();
  sharing_webrtc_connections_.emplace(sharing_connection_ptr,
                                      std::move(sharing_connection));
}

void SharingImpl::CreateNearbyConnections(
    mojo::PendingRemote<NearbyConnectionsHostMojom> host,
    CreateNearbyConnectionsCallback callback) {
  // Reset old instance of Nearby Connections stack.
  nearby_connections_.reset();

  mojo::PendingRemote<NearbyConnectionsMojom> remote;
  nearby_connections_ = std::make_unique<NearbyConnections>(
      remote.InitWithNewPipeAndPassReceiver(), std::move(host),
      base::BindOnce(&SharingImpl::NearbyConnectionsDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(std::move(remote));
}

void SharingImpl::NearbyConnectionsDisconnected() {
  nearby_connections_.reset();
}

size_t SharingImpl::GetWebRtcConnectionCountForTesting() const {
  return sharing_webrtc_connections_.size();
}

void SharingImpl::SharingWebRtcConnectionDisconnected(
    SharingWebRtcConnection* peer_connection) {
  sharing_webrtc_connections_.erase(peer_connection);
}

void SharingImpl::InitializeWebRtcFactory() {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  webrtc::PeerConnectionFactoryDependencies dependencies;
  dependencies.task_queue_factory = CreateWebRtcTaskQueueFactory();
  dependencies.network_thread = rtc::Thread::Current();
  dependencies.worker_thread = rtc::Thread::Current();
  dependencies.signaling_thread = rtc::Thread::Current();

  webrtc_peer_connection_factory_ =
      webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
}

}  // namespace sharing
