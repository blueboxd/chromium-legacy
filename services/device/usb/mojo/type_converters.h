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

namespace mojo {

template <>
struct TypeConverter<device::mojom::UsbIsochronousPacketPtr,
                     device::UsbDeviceHandle::IsochronousPacket> {
  static device::mojom::UsbIsochronousPacketPtr Convert(
      const device::UsbDeviceHandle::IsochronousPacket& packet);
};

}  // namespace mojo

#endif  // SERVICES_DEVICE_USB_MOJO_TYPE_CONVERTERS_H_
