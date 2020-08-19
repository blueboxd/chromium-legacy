// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_

#include <vector>

#include "base/sequence_checker.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/fido_device.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace device {
namespace cablev2 {

class Crypter;
class WebSocketAdapter;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoTunnelDevice : public FidoDevice {
 public:
  FidoTunnelDevice(network::mojom::NetworkContext* network_context,
                   const CableDiscoveryData::V2Data& v2data,
                   const CableEidArray& eid,
                   const CableEidArray& decrypted_eid);
  ~FidoTunnelDevice() override;

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;
  void Cancel(CancelToken token) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  enum class State {
    kConnecting,
    kConnected,
    kHandshakeProcessed,
    kReady,
    kError,
  };

  void OnTunnelReady(bool ok, base::Optional<uint8_t> shard_id);
  void OnTunnelData(base::Optional<base::span<const uint8_t>> data);
  void OnError();
  void MaybeFlushPendingMessage();

  State state_ = State::kConnecting;
  std::array<uint8_t, 8> id_;
  const CableDiscoveryData::V2Data v2data_;
  cablev2::NonceAndEID nonce_and_eid_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;
  std::vector<uint8_t> getinfo_response_bytes_;
  std::vector<uint8_t> pending_message_;
  DeviceCallback callback_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FidoTunnelDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoTunnelDevice);
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
