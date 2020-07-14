// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test/mock_bluetooth_adapter.h"

namespace bluetooth {

MockBluetoothAdapater::MockBluetoothAdapater() = default;

MockBluetoothAdapater::~MockBluetoothAdapater() = default;

void MockBluetoothAdapater::ConnectToDevice(const std::string& address,
                                            ConnectToDeviceCallback callback) {}

void MockBluetoothAdapater::GetDevices(GetDevicesCallback callback) {}

void MockBluetoothAdapater::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->present = present;
  std::move(callback).Run(std::move(adapter_info));
}

void MockBluetoothAdapater::SetClient(
    ::mojo::PendingRemote<mojom::AdapterClient> client) {}

void MockBluetoothAdapater::StartDiscoverySession(
    StartDiscoverySessionCallback callback) {}

}  // namespace bluetooth
