// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_unsigned_long.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_advertising_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"

namespace blink {

BluetoothAdvertisingEvent::BluetoothAdvertisingEvent(
    const AtomicString& event_type,
    const BluetoothAdvertisingEventInit* initializer)
    : Event(event_type, initializer),
      device_(initializer->device()),
      name_(initializer->name()),
      uuids_(initializer->uuids()),
      appearance_(initializer->hasAppearance() ? initializer->appearance() : 0),
      txPower_(initializer->hasTxPower() ? initializer->txPower() : 0),
      rssi_(initializer->hasRssi() ? initializer->rssi() : 0),
      manufacturer_data_map_(initializer->manufacturerData()),
      service_data_map_(initializer->serviceData()) {}

BluetoothAdvertisingEvent::BluetoothAdvertisingEvent(
    const AtomicString& event_type,
    BluetoothDevice* device,
    const String& name,
    const HeapVector<StringOrUnsignedLong>& uuids,
    base::Optional<uint16_t> appearance,
    base::Optional<int8_t> txPower,
    base::Optional<int8_t> rssi,
    BluetoothManufacturerDataMap* manufacturerData,
    BluetoothServiceDataMap* serviceData)
    : Event(event_type, Bubbles::kYes, Cancelable::kYes),
      device_(std::move(device)),
      name_(name),
      uuids_(uuids),
      appearance_(appearance),
      txPower_(txPower),
      rssi_(rssi),
      manufacturer_data_map_(manufacturerData),
      service_data_map_(serviceData) {}

BluetoothAdvertisingEvent::~BluetoothAdvertisingEvent() {}

void BluetoothAdvertisingEvent::Trace(Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(uuids_);
  visitor->Trace(manufacturer_data_map_);
  visitor->Trace(service_data_map_);
  Event::Trace(visitor);
}

const AtomicString& BluetoothAdvertisingEvent::InterfaceName() const {
  return event_type_names::kAdvertisementreceived;
}

BluetoothDevice* BluetoothAdvertisingEvent::device() const {
  return device_;
}

const String& BluetoothAdvertisingEvent::name() const {
  return name_;
}

const HeapVector<StringOrUnsignedLong>& BluetoothAdvertisingEvent::uuids()
    const {
  return uuids_;
}

uint16_t BluetoothAdvertisingEvent::appearance(bool& is_null) const {
  is_null = !appearance_.has_value();
  return appearance_.value_or(0);
}

int8_t BluetoothAdvertisingEvent::txPower(bool& is_null) const {
  is_null = !txPower_.has_value();
  return txPower_.value_or(0);
}

int8_t BluetoothAdvertisingEvent::rssi(bool& is_null) const {
  is_null = !rssi_.has_value();
  return rssi_.value_or(0);
}

BluetoothManufacturerDataMap* BluetoothAdvertisingEvent::manufacturerData()
    const {
  return manufacturer_data_map_;
}

BluetoothServiceDataMap* BluetoothAdvertisingEvent::serviceData() const {
  return service_data_map_;
}

}  // namespace blink
