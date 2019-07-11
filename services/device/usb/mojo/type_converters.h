// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_TYPE_CONVERTERS_H_
#define SERVICES_DEVICE_USB_MOJO_TYPE_CONVERTERS_H_

#include <vector>

#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/usb/usb_device_handle.h"

// Type converters to translate between internal device/usb data types and
// public Mojo interface data types. This must be included by any source file
// that uses these conversions explicitly or implicitly.

namespace device {
struct UsbConfigDescriptor;
class UsbDevice;
}  // namespace device

namespace mojo {

template <>
struct TypeConverter<device::mojom::UsbConfigurationInfoPtr,
                     device::UsbConfigDescriptor> {
  static device::mojom::UsbConfigurationInfoPtr Convert(
      const device::UsbConfigDescriptor& config);
};

template <>
struct TypeConverter<device::mojom::UsbDeviceInfoPtr, device::UsbDevice> {
  static device::mojom::UsbDeviceInfoPtr Convert(
      const device::UsbDevice& device);
};

template <>
struct TypeConverter<device::mojom::UsbIsochronousPacketPtr,
                     device::UsbDeviceHandle::IsochronousPacket> {
  static device::mojom::UsbIsochronousPacketPtr Convert(
      const device::UsbDeviceHandle::IsochronousPacket& packet);
};

template <typename A, typename B>
struct TypeConverter<std::vector<A>, std::vector<B>> {
  static std::vector<A> Convert(const std::vector<B>& input) {
    std::vector<A> result;
    result.reserve(input.size());
    for (const B& item : input)
      result.push_back(mojo::ConvertTo<A>(item));
    return result;
  }
};

}  // namespace mojo

#endif  // SERVICES_DEVICE_USB_MOJO_TYPE_CONVERTERS_H_
