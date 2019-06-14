// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_characteristics_finder.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;

namespace chromeos {

namespace secure_channel {

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder(
        scoped_refptr<BluetoothAdapter> adapter,
        BluetoothDevice* device,
        const RemoteAttribute& remote_service,
        const RemoteAttribute& to_peripheral_char,
        const RemoteAttribute& from_peripheral_char,
        const SuccessCallback& success_callback,
        const ErrorCallback& error_callback)
    : adapter_(adapter),
      bluetooth_device_(device),
      remote_service_(remote_service),
      to_peripheral_char_(to_peripheral_char),
      from_peripheral_char_(from_peripheral_char),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  adapter_->AddObserver(this);
  if (device->IsGattServicesDiscoveryComplete())
    ScanRemoteCharacteristics();
}

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder() {}

BluetoothLowEnergyCharacteristicsFinder::
    ~BluetoothLowEnergyCharacteristicsFinder() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void BluetoothLowEnergyCharacteristicsFinder::GattServicesDiscovered(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  // Ignore events about other devices.
  if (device != bluetooth_device_)
    return;
  PA_LOG(VERBOSE) << "All services discovered.";

  ScanRemoteCharacteristics();
}

void BluetoothLowEnergyCharacteristicsFinder::ScanRemoteCharacteristics() {
  for (const BluetoothRemoteGattService* service :
       bluetooth_device_->GetGattServices()) {
    if (service->GetUUID() != remote_service_.uuid)
      continue;

    std::vector<BluetoothRemoteGattCharacteristic*> tx_chars =
        service->GetCharacteristicsByUUID(to_peripheral_char_.uuid);
    std::vector<BluetoothRemoteGattCharacteristic*> rx_chars =
        service->GetCharacteristicsByUUID(from_peripheral_char_.uuid);
    if (tx_chars.empty()) {
      PA_LOG(WARNING) << "Service missing TX char.";
      continue;
    }

    if (rx_chars.empty()) {
      PA_LOG(WARNING) << "Service missing RX char.";
      continue;
    }

    NotifySuccess(service->GetIdentifier(), tx_chars.front()->GetIdentifier(),
                  rx_chars.front()->GetIdentifier());
    return;
  }

  if (has_error_callback_been_invoked_)
    return;
  // If all GATT services have been discovered and we haven't found the
  // characteristics we are looking for, call the error callback.
  has_error_callback_been_invoked_ = true;
  error_callback_.Run();
}

void BluetoothLowEnergyCharacteristicsFinder::NotifySuccess(
    std::string service_id,
    std::string tx_id,
    std::string rx_id) {
  from_peripheral_char_.id = rx_id;
  to_peripheral_char_.id = tx_id;
  remote_service_.id = service_id;
  success_callback_.Run(remote_service_, to_peripheral_char_,
                        from_peripheral_char_);
}

}  // namespace secure_channel

}  // namespace chromeos
