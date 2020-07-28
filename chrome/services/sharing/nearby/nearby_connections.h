// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "chrome/services/sharing/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/core_v2/core.h"

namespace location {
namespace nearby {
namespace connections {

// Implementation of the NearbyConnections mojo interface.
// This class acts as a bridge to the NearbyConnections library which is pulled
// in as a third_party dependency. It handles the translation from mojo calls to
// native callbacks and types that the library expects. This class runs in a
// sandboxed process and is called from the browser process. The passed |host|
// interface is implemented in the browser process and is used to fetch runtime
// dependencies to other mojo interfaces like Bluetooth or WiFi LAN.
class NearbyConnections : public mojom::NearbyConnections {
 public:
  // Creates a new instance of the NearbyConnections library. This will allocate
  // and initialize a new instance and hold on to the passed mojo pipes.
  // |on_disconnect| is called when either mojo interface disconnects and should
  // destroy this instance.
  NearbyConnections(
      mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
      mojom::NearbyConnectionsDependenciesPtr dependencies,
      base::OnceClosure on_disconnect,
      std::unique_ptr<Core> core = std::make_unique<Core>());

  NearbyConnections(const NearbyConnections&) = delete;
  NearbyConnections& operator=(const NearbyConnections&) = delete;
  ~NearbyConnections() override;

  // Should only be used by objects within lifetime of NearbyConnections.
  static NearbyConnections& GetInstance();

  bluetooth::mojom::Adapter* GetBluetoothAdapter();
  network::mojom::P2PSocketManager* GetWebRtcP2PSocketManager();
  network::mojom::MdnsResponder* GetWebRtcMdnsResponder();
  sharing::mojom::IceConfigFetcher* GetWebRtcIceConfigFetcher();
  sharing::mojom::WebRtcSignalingMessenger* GetWebRtcSignalingMessenger();

  // mojom::NearbyConnections:
  void StartDiscovery(
      const std::string& service_id,
      mojom::DiscoveryOptionsPtr options,
      mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
      StartDiscoveryCallback callback) override;
  void StopDiscovery(StopDiscoveryCallback callback) override;

 private:
  void OnDisconnect();

  mojo::Receiver<mojom::NearbyConnections> nearby_connections_;
  base::OnceClosure on_disconnect_;

  // Medium dependencies. SharedRemote is used to ensure all calls are posted
  // to sequence binding the Remote.
  mojo::SharedRemote<bluetooth::mojom::Adapter> bluetooth_adapter_;
  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  mojo::SharedRemote<network::mojom::MdnsResponder> mdns_responder_;
  mojo::SharedRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>
      webrtc_signaling_messenger_;

  std::unique_ptr<Core> core_;

  base::WeakPtrFactory<NearbyConnections> weak_ptr_factory_{this};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
