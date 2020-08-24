// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_peripheral.h"

namespace location {
namespace nearby {
namespace chrome {

BlePeripheral::BlePeripheral(api::BluetoothDevice& bluetooth_device)
    : bluetooth_device_(bluetooth_device) {}

BlePeripheral::~BlePeripheral() = default;

api::BluetoothDevice& BlePeripheral::GetBluetoothDevice() {
  return bluetooth_device_;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
