// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"

namespace bluetooth {

FakeAdapter::FakeAdapter() = default;

FakeAdapter::~FakeAdapter() = default;

void FakeAdapter::ConnectToDevice(const std::string& address,
                                  ConnectToDeviceCallback callback) {}

void FakeAdapter::GetDevices(GetDevicesCallback callback) {}

void FakeAdapter::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->name = name_;
  adapter_info->present = present_;
  adapter_info->powered = powered_;
  adapter_info->discoverable = discoverable_;
  adapter_info->discovering = discovering_;
  std::move(callback).Run(std::move(adapter_info));
}

void FakeAdapter::SetClient(
    ::mojo::PendingRemote<mojom::AdapterClient> client) {}

void FakeAdapter::StartDiscoverySession(
    StartDiscoverySessionCallback callback) {}

void FakeAdapter::ConnectToServiceInsecurely(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    ConnectToServiceInsecurelyCallback callback) {}

}  // namespace bluetooth
