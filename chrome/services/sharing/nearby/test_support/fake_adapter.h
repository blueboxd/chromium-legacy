// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace bluetooth {

class FakeAdapter : public mojom::Adapter {
 public:
  FakeAdapter();
  FakeAdapter(const FakeAdapter&) = delete;
  FakeAdapter& operator=(const FakeAdapter&) = delete;
  ~FakeAdapter() override;

  // mojom::Adapter
  void ConnectToDevice(const std::string& address,
                       ConnectToDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void GetInfo(GetInfoCallback callback) override;
  void SetClient(::mojo::PendingRemote<mojom::AdapterClient> client,
                 SetClientCallback callback) override;
  void StartDiscoverySession(StartDiscoverySessionCallback callback) override;
  void ConnectToServiceInsecurely(
      const std::string& address,
      const device::BluetoothUUID& service_uuid,
      ConnectToServiceInsecurelyCallback callback) override;

  void SetShouldDiscoverySucceed(bool should_discovery_succeed);
  void SetDiscoverySessionDestroyedCallback(base::OnceClosure callback);
  bool IsDiscoverySessionActive();
  void NotifyDeviceAdded(mojom::DeviceInfoPtr device_info);
  void NotifyDeviceChanged(mojom::DeviceInfoPtr device_info);
  void NotifyDeviceRemoved(mojom::DeviceInfoPtr device_info);

  mojo::Receiver<mojom::Adapter> adapter_{this};
  std::string name_ = "AdapterName";
  bool present_ = true;
  bool powered_ = true;
  bool discoverable_ = false;
  bool discovering_ = false;

 private:
  void OnDiscoverySessionDestroyed();

  mojom::DiscoverySession* discovery_session_ = nullptr;
  bool should_discovery_succeed_ = true;
  base::OnceClosure on_discovery_session_destroyed_callback_;

  mojo::Remote<mojom::AdapterClient> client_;
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_
