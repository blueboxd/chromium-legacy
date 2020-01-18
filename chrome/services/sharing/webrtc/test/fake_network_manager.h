// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_NETWORK_MANAGER_H_
#define CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_NETWORK_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/network.h"

namespace sharing {

// FakeNetworkManager always returns one interface with the IP address
// specified in the constructor.
class FakeNetworkManager : public rtc::NetworkManager {
 public:
  explicit FakeNetworkManager(const rtc::IPAddress& address);
  FakeNetworkManager(const FakeNetworkManager&) = delete;
  FakeNetworkManager& operator=(const FakeNetworkManager&) = delete;
  ~FakeNetworkManager() override;

  // rtc::NetworkManager interface.
  void StartUpdating() override;
  void StopUpdating() override;
  void GetNetworks(NetworkList* networks) const override;

 protected:
  void SendNetworksChangedSignal();

  std::unique_ptr<rtc::Network> network_;

  base::WeakPtrFactory<FakeNetworkManager> weak_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_NETWORK_MANAGER_H_
