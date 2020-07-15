// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/mock_bluetooth_adapter.h"

namespace bluetooth {

MockBluetoothAdapter::MockBluetoothAdapter() = default;

MockBluetoothAdapter::~MockBluetoothAdapter() = default;

void MockBluetoothAdapter::ConnectToDevice(const std::string& address,
                                           ConnectToDeviceCallback callback) {}

void MockBluetoothAdapter::GetDevices(GetDevicesCallback callback) {}

void MockBluetoothAdapter::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->present = present;
  std::move(callback).Run(std::move(adapter_info));
}

void MockBluetoothAdapter::SetClient(
    ::mojo::PendingRemote<mojom::AdapterClient> client) {}

void MockBluetoothAdapter::StartDiscoverySession(
    StartDiscoverySessionCallback callback) {}

}  // namespace bluetooth
