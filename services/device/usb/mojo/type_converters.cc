// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

namespace mojo {

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
