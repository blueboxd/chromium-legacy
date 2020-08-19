// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_tunnel_device.h"

#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/fido_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace device {
namespace cablev2 {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cablev2_websocket_from_client", R"(
        semantics {
          sender: "Phone as a Security Key"
          description:
            "Chrome can communicate with a phone for the purpose of using "
            "the phone as a security key. This WebSocket connection is made to "
            "a rendezvous service of the phone's choosing. Mostly likely that "
            "is a Google service because the phone-side is being handled by "
            "Chrome on that device. The service carries only end-to-end "
            "encrypted data where the keys are shared directly between the "
            "client and phone via QR code and Bluetooth broadcast."
          trigger:
            "A web-site initiates a WebAuthn request and the user scans a QR "
            "code with their phone."
          data: "Only encrypted data that the service does not have the keys "
                "for."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Not controlled by a setting because the operation is "
            "triggered by significant user action."
          policy_exception_justification:
            "No policy provided because the operation is triggered by "
            " significant user action."
        })");

FidoTunnelDevice::FidoTunnelDevice(
    network::mojom::NetworkContext* network_context,
    const CableDiscoveryData::V2Data& v2data,
    const CableEidArray& eid,
    const CableEidArray& decrypted_eid)
    : v2data_(v2data) {
  DCHECK(eid::IsValid(decrypted_eid));
  crypto::RandBytes(id_);

  const eid::Components components = eid::ToComponents(decrypted_eid);
  nonce_and_eid_.first = components.nonce;
  nonce_and_eid_.second = eid;

  std::array<uint8_t, 16> tunnel_id;
  bool ok = HKDF(tunnel_id.data(), tunnel_id.size(), EVP_sha256(),
                 v2data_.tunnel_id_gen_key.data(),
                 v2data_.tunnel_id_gen_key.size(), components.nonce.data(),
                 components.nonce.size(), /*info=*/nullptr, 0);
  DCHECK(ok);

  const GURL url(cablev2::tunnelserver::GetURL(
      components.tunnel_server_domain, cablev2::tunnelserver::Action::kConnect,
      tunnel_id));
  FIDO_LOG(DEBUG) << "Connecting caBLEv2 tunnel: " << url
                  << " shard: " << static_cast<int>(components.shard_id);

  websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
      base::BindOnce(&FidoTunnelDevice::OnTunnelReady, base::Unretained(this)),
      base::BindRepeating(&FidoTunnelDevice::OnTunnelData,
                          base::Unretained(this)));
  network_context->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::IsolationInfo(), /*additional_headers=*/{},
      network::mojom::kBrowserProcessId,
      /*render_frame_id=*/0, url::Origin::Create(url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(), mojo::NullRemote(),
      mojo::NullRemote());
}

FidoTunnelDevice::~FidoTunnelDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FidoDevice::CancelToken FidoTunnelDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);

  pending_message_ = std::move(command);
  callback_ = std::move(callback);
  if (state_ == State::kHandshakeProcessed || state_ == State::kReady) {
    MaybeFlushPendingMessage();
  }

  // TODO: cancelation would be useful, but it depends on the GMSCore action
  // being cancelable on Android, which it currently is not.
  return kInvalidCancelToken + 1;
}

void FidoTunnelDevice::Cancel(CancelToken token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string FidoTunnelDevice::GetId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "tunnel-" + base::HexEncode(id_);
}

FidoTransportProtocol FidoTunnelDevice::DeviceTransport() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
}

base::WeakPtr<FidoDevice> FidoTunnelDevice::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void FidoTunnelDevice::OnTunnelReady(bool ok,
                                     base::Optional<uint8_t> shard_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kConnecting, state_);

  if (!ok) {
    OnError();
    return;
  }

  state_ = State::kConnected;
}

void FidoTunnelDevice::OnTunnelData(
    base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!data) {
    OnError();
    return;
  }

  switch (state_) {
    case State::kError:
    case State::kConnecting:
      NOTREACHED();
      break;

    case State::kConnected: {
      std::vector<uint8_t> response;
      base::Optional<std::pair<std::unique_ptr<Crypter>, std::vector<uint8_t>>>
          result(cablev2::RespondToHandshake(
              v2data_.psk_gen_key, nonce_and_eid_, v2data_.local_identity_seed,
              base::nullopt, *data, &response));
      if (!result || result->second.empty()) {
        FIDO_LOG(ERROR) << "caBLEv2 handshake failed";
        OnError();
        return;
      }

      FIDO_LOG(DEBUG) << "caBLEv2 handshake successful";
      websocket_client_->Write(response);
      crypter_ = std::move(result->first);
      getinfo_response_bytes_ = std::move(result->second);
      state_ = State::kHandshakeProcessed;

      MaybeFlushPendingMessage();
      break;
    }

    case State::kHandshakeProcessed: {
      // This is the post-handshake message that optionally contains pairing
      // information.
      std::vector<uint8_t> decrypted;
      if (!crypter_->Decrypt(*data, &decrypted)) {
        FIDO_LOG(ERROR) << "decryption failed for caBLE pairing message";
        OnError();
        return;
      }
      base::Optional<cbor::Value> payload = DecodePaddedCBORMap(decrypted);
      if (!payload) {
        FIDO_LOG(ERROR) << "decode failed for caBLE pairing message";
        OnError();
        return;
      }

      // TODO: pairing not yet handled.

      state_ = State::kReady;
      break;
    }

    case State::kReady: {
      if (!callback_) {
        OnError();
        return;
      }

      std::vector<uint8_t> plaintext;
      if (!crypter_->Decrypt(*data, &plaintext)) {
        FIDO_LOG(ERROR) << "decryption failed for caBLE message";
        OnError();
        return;
      }

      std::move(callback_).Run(std::move(plaintext));
      break;
    }
  }
}

void FidoTunnelDevice::OnError() {
  state_ = State::kError;
  websocket_client_.reset();
  if (callback_) {
    std::move(callback_).Run(base::nullopt);
  }
}

void FidoTunnelDevice::MaybeFlushPendingMessage() {
  if (pending_message_.empty()) {
    return;
  }
  std::vector<uint8_t> pending(std::move(pending_message_));

  if (pending.size() == 1 &&
      pending[0] ==
          static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetInfo)) {
    DCHECK(!getinfo_response_bytes_.empty());
    std::vector<uint8_t> reply;
    reply.reserve(1 + getinfo_response_bytes_.size());
    reply.push_back(static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
    reply.insert(reply.end(), getinfo_response_bytes_.begin(),
                 getinfo_response_bytes_.end());
    std::move(callback_).Run(std::move(reply));
  } else if (crypter_->Encrypt(&pending)) {
    websocket_client_->Write(pending);
  }
}

}  // namespace cablev2
}  // namespace device
