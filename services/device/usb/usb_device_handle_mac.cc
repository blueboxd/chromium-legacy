// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_mac.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <utility>

#include "base/mac/scoped_ioplugininterface.h"
#include "services/device/usb/usb_device_mac.h"

namespace device {

UsbDeviceHandleMac::UsbDeviceHandleMac(
    scoped_refptr<UsbDeviceMac> device,
    base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface182>
        device_interface)
    : device_interface_(std::move(device_interface)),
      device_(std::move(device)) {}

UsbDeviceHandleMac::~UsbDeviceHandleMac() {}

scoped_refptr<UsbDevice> UsbDeviceHandleMac::GetDevice() const {
  return device_;
}

void UsbDeviceHandleMac::Close() {
  GetDevice()->HandleClosed(this);
  device_ = nullptr;
}

void UsbDeviceHandleMac::SetConfiguration(int configuration_value,
                                          ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::ClaimInterface(int interface_number,
                                        ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::ReleaseInterface(int interface_number,
                                          ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::SetInterfaceAlternateSetting(int interface_number,
                                                      int alternate_setting,
                                                      ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::ResetDevice(ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::ClearHalt(mojom::UsbTransferDirection direction,
                                   uint8_t endpoint_number,
                                   ResultCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::ControlTransfer(
    mojom::UsbTransferDirection direction,
    mojom::UsbControlTransferType request_type,
    mojom::UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::IsochronousTransferIn(
    uint8_t endpoint,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::IsochronousTransferOut(
    uint8_t endpoint,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
  return;
}

void UsbDeviceHandleMac::GenericTransfer(
    mojom::UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  NOTIMPLEMENTED();
  return;
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleMac::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace device
