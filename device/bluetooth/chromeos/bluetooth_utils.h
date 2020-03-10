// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class TimeDelta;
}  // namespace base

// This file contains common utilities, including filtering bluetooth devices
// based on the filter criteria.
namespace device {

enum class BluetoothFilterType {
  // No filtering, all bluetooth devices will be returned.
  ALL = 0,
  // Return bluetooth devices that are known to the UI.
  // I.e. bluetooth device type != UNKNOWN
  KNOWN,
};

enum class BluetoothUiSurface {
  kSettings,
  kSystemTray,
};

// Return filtered devices based on the filter type and max number of devices.
DEVICE_BLUETOOTH_EXPORT device::BluetoothAdapter::DeviceList
FilterBluetoothDeviceList(const BluetoothAdapter::DeviceList& devices,
                          BluetoothFilterType filter_type,
                          int max_devices);

// Record outcome of user attempting to pair to a device.
DEVICE_BLUETOOTH_EXPORT void RecordPairingResult(bool success,
                                                 BluetoothTransport transport,
                                                 base::TimeDelta duration);

// Record outcome of user attempting to reconnect to a previously paired device.
DEVICE_BLUETOOTH_EXPORT void RecordUserInitiatedReconnectionAttemptResult(
    bool success,
    BluetoothUiSurface surface);

// Record how long it took for a user to find and select the device they wished
// to connect to.
DEVICE_BLUETOOTH_EXPORT void RecordDeviceSelectionDuration(
    base::TimeDelta duration,
    BluetoothUiSurface surface,
    bool was_paired,
    BluetoothTransport transport);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
