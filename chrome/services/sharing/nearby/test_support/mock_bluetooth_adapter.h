// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_MOCK_BLUETOOTH_ADAPTER_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_MOCK_BLUETOOTH_ADAPTER_H_

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bluetooth {

class MockBluetoothAdapter : public mojom::Adapter {
 public:
  MockBluetoothAdapter();
  MockBluetoothAdapter(const MockBluetoothAdapter&) = delete;
  MockBluetoothAdapter& operator=(const MockBluetoothAdapter&) = delete;
  ~MockBluetoothAdapter() override;

  // mojom::Adapter
  void ConnectToDevice(const std::string& address,
                       ConnectToDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void GetInfo(GetInfoCallback callback) override;
  void SetClient(::mojo::PendingRemote<mojom::AdapterClient> client) override;
  void StartDiscoverySession(StartDiscoverySessionCallback callback) override;

  mojo::Receiver<mojom::Adapter> adapter{this};
  bool present = true;
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_MOCK_BLUETOOTH_ADAPTER_H_
