// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_medium.h"

namespace location {
namespace nearby {
namespace chrome {

BleMedium::BleMedium() = default;

BleMedium::~BleMedium() = default;

bool BleMedium::StartAdvertising(absl::string_view service_id,
                                 const ByteArray& advertisement) {
  // TODO(b/154845685): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

void BleMedium::StopAdvertising(absl::string_view service_id) {
  // TODO(b/154845685): Implement this method.
  NOTIMPLEMENTED();
}

bool BleMedium::StartScanning(
    absl::string_view service_id,
    const api::BleMedium::DiscoveredPeripheralCallback&
        discovered_peripheral_callback) {
  // TODO(b/154848193): Implement this method.
  NOTIMPLEMENTED();
  return true;
}

void BleMedium::StopScanning(absl::string_view service_id) {
  // TODO(b/154848193): Implement this method.
  NOTIMPLEMENTED();
}

bool BleMedium::StartAcceptingConnections(
    absl::string_view service_id,
    const api::BleMedium::AcceptedConnectionCallback&
        accepted_connection_callback) {
  // Do not actually start a GATT server, because BLE connections are not yet
  // supported in Chrome Nearby. However, return true in order to allow
  // BLE advertising to continue.

  // TODO(hansberry): Verify if this is still required in NCv2.
  return true;
}

void BleMedium::StopAcceptingConnections(const std::string& service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
}

std::unique_ptr<api::BleSocket> BleMedium::Connect(
    api::BlePeripheral* ble_peripheral,
    absl::string_view service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
  return nullptr;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
