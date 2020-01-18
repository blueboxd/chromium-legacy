// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_TEST_MOCK_SHARING_CONNECTION_HOST_H_
#define CHROME_SERVICES_SHARING_WEBRTC_TEST_MOCK_SHARING_CONNECTION_HOST_H_

#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sharing {

class MockSharingConnectionHost : public mojom::SignallingSender,
                                  public mojom::SharingWebRtcConnectionDelegate,
                                  public network::mojom::P2PSocketManager,
                                  public network::mojom::MdnsResponder {
 public:
  MockSharingConnectionHost();
  MockSharingConnectionHost(const MockSharingConnectionHost&) = delete;
  MockSharingConnectionHost& operator=(const MockSharingConnectionHost&) =
      delete;
  ~MockSharingConnectionHost() override;

  // mojom::SignallingSender:
  MOCK_METHOD2(SendOffer, void(const std::string&, SendOfferCallback));
  MOCK_METHOD1(SendIceCandidates, void(std::vector<mojom::IceCandidatePtr>));

  // mojom::SharingWebRtcConnectionDelegate:
  MOCK_METHOD1(OnMessageReceived, void(const std::vector<uint8_t>&));

  // network::mojom::P2PSocketManager:
  MOCK_METHOD1(
      StartNetworkNotifications,
      void(mojo::PendingRemote<network::mojom::P2PNetworkNotificationClient>));
  MOCK_METHOD3(GetHostAddress,
               void(const std::string&, bool, GetHostAddressCallback));
  MOCK_METHOD6(CreateSocket,
               void(::network::P2PSocketType,
                    const ::net::IPEndPoint&,
                    const ::network::P2PPortRange&,
                    const ::network::P2PHostAndIPEndPoint&,
                    mojo::PendingRemote<network::mojom::P2PSocketClient>,
                    mojo::PendingReceiver<network::mojom::P2PSocket>));

  // network::mojom::MdnsResponder:
  MOCK_METHOD2(CreateNameForAddress,
               void(const ::net::IPAddress&, CreateNameForAddressCallback));
  MOCK_METHOD2(RemoveNameForAddress,
               void(const ::net::IPAddress&, RemoveNameForAddressCallback));

  mojo::Receiver<mojom::SignallingSender> signalling_sender{this};
  mojo::Remote<mojom::SignallingReceiver> signalling_receiver;
  mojo::Receiver<mojom::SharingWebRtcConnectionDelegate> delegate{this};
  mojo::Remote<mojom::SharingWebRtcConnection> connection;
  mojo::Receiver<network::mojom::P2PSocketManager> socket_manager{this};
  mojo::Receiver<network::mojom::MdnsResponder> mdns_responder{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_TEST_MOCK_SHARING_CONNECTION_HOST_H_
