// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_mac.h"

#include <utility>

#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle.h"

namespace device {

UsbDeviceMac::UsbDeviceMac(uint64_t entry_id,
                           mojom::UsbDeviceInfoPtr device_info)
    : UsbDevice(std::move(device_info)), entry_id_(entry_id) {}

UsbDeviceMac::~UsbDeviceMac() = default;

void UsbDeviceMac::Open(OpenCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace device
