// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_media_endpoint_service_provider.h"

#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_transport_client.h"

using dbus::ObjectPath;

namespace bluez {

FakeBluetoothMediaEndpointServiceProvider::
    FakeBluetoothMediaEndpointServiceProvider(const ObjectPath& object_path,
                                              Delegate* delegate)
    : object_path_(object_path), delegate_(delegate) {
  DVLOG(1) << "Create Bluetooth Media Endpoint: " << object_path_.value();
}

FakeBluetoothMediaEndpointServiceProvider::
    ~FakeBluetoothMediaEndpointServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth Media Endpoint: " << object_path_.value();
}

void FakeBluetoothMediaEndpointServiceProvider::SetConfiguration(
    const ObjectPath& transport_path,
    const Delegate::TransportProperties& properties) {
  DVLOG(1) << object_path_.value() << ": SetConfiguration for "
           << transport_path.value();

  delegate_->SetConfiguration(transport_path, properties);
}

void FakeBluetoothMediaEndpointServiceProvider::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const Delegate::SelectConfigurationCallback& callback) {
  DVLOG(1) << object_path_.value() << ": SelectConfiguration";

  delegate_->SelectConfiguration(capabilities, callback);

  // Makes the transport object valid for the given endpoint path.
  FakeBluetoothMediaTransportClient* transport =
      static_cast<FakeBluetoothMediaTransportClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothMediaTransportClient());
  DCHECK(transport);
  transport->SetValid(this, true);
}

void FakeBluetoothMediaEndpointServiceProvider::ClearConfiguration(
    const ObjectPath& transport_path) {
  DVLOG(1) << object_path_.value() << ": ClearConfiguration on "
           << transport_path.value();

  delegate_->ClearConfiguration(transport_path);
}

void FakeBluetoothMediaEndpointServiceProvider::Released() {
  DVLOG(1) << object_path_.value() << ": Released";

  delegate_->Released();
}

}  // namespace bluez
