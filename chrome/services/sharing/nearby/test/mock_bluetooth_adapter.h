// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_BLUETOOTH_ADAPTER_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_BLUETOOTH_ADAPTER_H_

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bluetooth {

class MockBluetoothAdapater : public mojom::Adapter {
 public:
  MockBluetoothAdapater();
  MockBluetoothAdapater(const MockBluetoothAdapater&) = delete;
  MockBluetoothAdapater& operator=(const MockBluetoothAdapater&) = delete;
  ~MockBluetoothAdapater() override;

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

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_BLUETOOTH_ADAPTER_H_
