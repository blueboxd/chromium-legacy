// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility routines for working with UsbDevices that are printers.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_UTIL_H__
#define CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_UTIL_H__

#include <memory>

#include "services/device/public/mojom/usb_device.mojom.h"

namespace chromeos {

class Printer;

base::string16 GetManufacturerName(
    const device::mojom::UsbDeviceInfo& device_info);

base::string16 GetProductName(const device::mojom::UsbDeviceInfo& device_info);

base::string16 GetSerialNumber(const device::mojom::UsbDeviceInfo& device_info);

bool UsbDeviceIsPrinter(const device::mojom::UsbDeviceInfo& device_info);

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices.
//
// Returns nullptr and logs an error on failure.
std::unique_ptr<Printer> UsbDeviceToPrinter(
    const device::mojom::UsbDeviceInfo& device_info);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_UTIL_H__
