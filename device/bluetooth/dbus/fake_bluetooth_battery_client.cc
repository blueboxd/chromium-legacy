// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_battery_client.h"

#include "base/logging.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothBatteryClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothBatteryClient::Properties(
          nullptr,
          bluetooth_battery::kBluetoothBatteryInterface,
          callback) {}

FakeBluetoothBatteryClient::Properties::~Properties() = default;

void FakeBluetoothBatteryClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothBatteryClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothBatteryClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothBatteryClient::FakeBluetoothBatteryClient() = default;

FakeBluetoothBatteryClient::~FakeBluetoothBatteryClient() = default;

void FakeBluetoothBatteryClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothBatteryClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothBatteryClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath>
FakeBluetoothBatteryClient::GetBatteriesForAdapter(
    const dbus::ObjectPath& adapter_path) {
  if (adapter_path ==
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath))
    return battery_list_;
  else
    return std::vector<dbus::ObjectPath>();
}

FakeBluetoothBatteryClient::Properties*
FakeBluetoothBatteryClient::GetProperties(const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second.get();
  return nullptr;
}

}  // namespace bluez
