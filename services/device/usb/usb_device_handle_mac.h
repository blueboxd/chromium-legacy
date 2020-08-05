// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_

#include "services/device/usb/usb_device_handle.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <vector>

#include "base/mac/scoped_ioplugininterface.h"
#include "base/memory/ref_counted.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

class UsbDeviceMac;

class UsbDeviceHandleMac : public UsbDeviceHandle {
 public:
  UsbDeviceHandleMac(const UsbDeviceHandleMac&) = delete;
  UsbDeviceHandleMac& operator=(const UsbDeviceHandleMac&) = delete;

  // UsbDeviceHandle implementation:
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override;
  void ClaimInterface(int interface_number, ResultCallback callback) override;
  void ReleaseInterface(int interface_number, ResultCallback callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override;
  void ResetDevice(ResultCallback callback) override;
  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ResultCallback callback) override;
  void ControlTransfer(mojom::UsbTransferDirection direction,
                       mojom::UsbControlTransferType request_type,
                       mojom::UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
  void IsochronousTransferIn(uint8_t endpoint,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override;
  void IsochronousTransferOut(uint8_t endpoint,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override;
  void GenericTransfer(mojom::UsbTransferDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
  const mojom::UsbInterfaceInfo* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

  UsbDeviceHandleMac(scoped_refptr<UsbDeviceMac> device,
                     base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface182>
                         device_interface);

 protected:
  ~UsbDeviceHandleMac() override;
  friend class UsbDeviceMac;

 private:
  base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface182> device_interface_;
  scoped_refptr<UsbDeviceMac> device_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_
