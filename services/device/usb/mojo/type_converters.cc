// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device.h"

namespace mojo {

// static
device::mojom::UsbConfigurationInfoPtr TypeConverter<
    device::mojom::UsbConfigurationInfoPtr,
    device::UsbConfigDescriptor>::Convert(const device::UsbConfigDescriptor&
                                              config) {
  auto info = device::mojom::UsbConfigurationInfo::New();
  info->configuration_value = config.configuration_value;
  info->self_powered = config.self_powered;
  info->remote_wakeup = config.remote_wakeup;
  info->maximum_power = config.maximum_power;
  info->extra_data.assign(config.extra_data.begin(), config.extra_data.end());

  info->interfaces.reserve(config.interfaces.size());
  for (const auto& interface : config.interfaces) {
    info->interfaces.push_back(interface->Clone());
  }

  return info;
}

// static
device::mojom::UsbDeviceInfoPtr
TypeConverter<device::mojom::UsbDeviceInfoPtr, device::UsbDevice>::Convert(
    const device::UsbDevice& device) {
  auto info = device::mojom::UsbDeviceInfo::New();
  info->guid = device.guid();
  info->usb_version_major = device.usb_version() >> 8;
  info->usb_version_minor = device.usb_version() >> 4 & 0xf;
  info->usb_version_subminor = device.usb_version() & 0xf;
  info->class_code = device.device_class();
  info->subclass_code = device.device_subclass();
  info->protocol_code = device.device_protocol();
  info->bus_number = device.bus_number();
  info->port_number = device.port_number();
  info->vendor_id = device.vendor_id();
  info->product_id = device.product_id();
  info->device_version_major = device.device_version() >> 8;
  info->device_version_minor = device.device_version() >> 4 & 0xf;
  info->device_version_subminor = device.device_version() & 0xf;
  info->manufacturer_name = device.manufacturer_string();
  info->product_name = device.product_string();
  info->serial_number = device.serial_number();
  info->webusb_landing_page = device.webusb_landing_page();
  const device::UsbConfigDescriptor* config = device.active_configuration();
  info->active_configuration = config ? config->configuration_value : 0;
  info->configurations =
      mojo::ConvertTo<std::vector<device::mojom::UsbConfigurationInfoPtr>>(
          device.configurations());
  return info;
}

// static
device::mojom::UsbIsochronousPacketPtr
TypeConverter<device::mojom::UsbIsochronousPacketPtr,
              device::UsbDeviceHandle::IsochronousPacket>::
    Convert(const device::UsbDeviceHandle::IsochronousPacket& packet) {
  auto info = device::mojom::UsbIsochronousPacket::New();
  info->length = packet.length;
  info->transferred_length = packet.transferred_length;
  info->status =
      mojo::ConvertTo<device::mojom::UsbTransferStatus>(packet.status);
  return info;
}

}  // namespace mojo
