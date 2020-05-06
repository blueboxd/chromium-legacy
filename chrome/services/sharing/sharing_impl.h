// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_SHARING_IMPL_H_
#define CHROME_SERVICES_SHARING_SHARING_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom-forward.h"
#include "chrome/services/sharing/public/mojom/sharing.mojom.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/mdns_responder.mojom-forward.h"
#include "services/network/public/mojom/p2p.mojom-forward.h"

namespace webrtc {
class PeerConnectionFactoryInterface;
}  // namespace webrtc

namespace location {
namespace nearby {
namespace connections {
class NearbyConnections;
}  // namespace connections
}  // namespace nearby
}  // namespace location

namespace sharing {

class SharingWebRtcConnection;

class SharingImpl : public mojom::Sharing {
 public:
  using NearbyConnectionsHostMojom =
      location::nearby::connections::mojom::NearbyConnectionsHost;
  using NearbyConnectionsMojom =
      location::nearby::connections::mojom::NearbyConnections;
  using NearbyConnections = location::nearby::connections::NearbyConnections;

  explicit SharingImpl(mojo::PendingReceiver<mojom::Sharing> receiver);
  SharingImpl(const SharingImpl&) = delete;
  SharingImpl& operator=(const SharingImpl&) = delete;
  ~SharingImpl() override;

  // mojom::Sharing:
  void CreateSharingWebRtcConnection(
      mojo::PendingRemote<mojom::SignallingSender> signalling_sender,
      mojo::PendingReceiver<mojom::SignallingReceiver> signalling_receiver,
      mojo::PendingRemote<mojom::SharingWebRtcConnectionDelegate> delegate,
      mojo::PendingReceiver<mojom::SharingWebRtcConnection> connection,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
      std::vector<mojom::IceServerPtr> ice_servers) override;
  void CreateNearbyConnections(
      mojo::PendingRemote<NearbyConnectionsHostMojom> host,
      CreateNearbyConnectionsCallback callback) override;

  size_t GetWebRtcConnectionCountForTesting() const;

 private:
  void InitializeWebRtcFactory();

  void SharingWebRtcConnectionDisconnected(SharingWebRtcConnection* connection);

  void NearbyConnectionsDisconnected();

  mojo::Receiver<mojom::Sharing> receiver_;

  std::map<SharingWebRtcConnection*, std::unique_ptr<SharingWebRtcConnection>>
      sharing_webrtc_connections_;
  scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      webrtc_peer_connection_factory_;

  std::unique_ptr<NearbyConnections> nearby_connections_;

  base::WeakPtrFactory<SharingImpl> weak_ptr_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_SHARING_IMPL_H_
