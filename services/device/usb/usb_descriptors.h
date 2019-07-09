// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_
#define SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

class UsbDeviceHandle;

using UsbTransferType = mojom::UsbTransferType;
using UsbTransferDirection = mojom::UsbTransferDirection;
using UsbSynchronizationType = mojom::UsbSynchronizationType;
using UsbUsageType = mojom::UsbUsageType;
using UsbEndpointInfoPtr = mojom::UsbEndpointInfoPtr;

struct UsbInterfaceDescriptor {
  explicit UsbInterfaceDescriptor(const uint8_t* data);
  UsbInterfaceDescriptor(uint8_t interface_number,
                         uint8_t alternate_setting,
                         uint8_t interface_class,
                         uint8_t interface_subclass,
                         uint8_t interface_protocol);
  UsbInterfaceDescriptor() = delete;
  UsbInterfaceDescriptor(const UsbInterfaceDescriptor& other);
  UsbInterfaceDescriptor(UsbInterfaceDescriptor&& other);
  ~UsbInterfaceDescriptor();

  uint8_t interface_number;
  uint8_t alternate_setting;
  uint8_t interface_class;
  uint8_t interface_subclass;
  uint8_t interface_protocol;
  std::vector<UsbEndpointInfoPtr> endpoints;
  std::vector<uint8_t> extra_data;
  // First interface of the function to which this interface belongs.
  uint8_t first_interface;
};

struct UsbConfigDescriptor {
  explicit UsbConfigDescriptor(const uint8_t* data);
  UsbConfigDescriptor(uint8_t configuration_value,
                      bool self_powered,
                      bool remote_wakeup,
                      uint8_t maximum_power);
  UsbConfigDescriptor() = delete;
  UsbConfigDescriptor(const UsbConfigDescriptor& other);
  UsbConfigDescriptor(UsbConfigDescriptor&& other);
  ~UsbConfigDescriptor();

  // Scans through |extra_data| for interface association descriptors and
  // populates |first_interface| for each interface in this configuration.
  void AssignFirstInterfaceNumbers();

  uint8_t configuration_value;
  bool self_powered;
  bool remote_wakeup;
  uint8_t maximum_power;
  std::vector<UsbInterfaceDescriptor> interfaces;
  std::vector<uint8_t> extra_data;
};

struct UsbDeviceDescriptor {
  UsbDeviceDescriptor();
  UsbDeviceDescriptor(const UsbDeviceDescriptor& other);
  UsbDeviceDescriptor(UsbDeviceDescriptor&& other);
  ~UsbDeviceDescriptor();

  UsbDeviceDescriptor& operator=(UsbDeviceDescriptor&& other);

  // Parses |buffer| for USB descriptors. Any configuration descriptors found
  // will be added to |configurations|. If a device descriptor is found it will
  // be used to populate this struct's fields. This function may be called more
  // than once (i.e. for multiple buffers containing a configuration descriptor
  // each).
  bool Parse(const std::vector<uint8_t>& buffer);

  uint16_t usb_version = 0;
  uint8_t device_class = 0;
  uint8_t device_subclass = 0;
  uint8_t device_protocol = 0;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
  uint16_t device_version = 0;
  uint8_t i_manufacturer = 0;
  uint8_t i_product = 0;
  uint8_t i_serial_number = 0;
  uint8_t num_configurations = 0;
  std::vector<UsbConfigDescriptor> configurations;
};

void ReadUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    base::OnceCallback<void(std::unique_ptr<UsbDeviceDescriptor>)> callback);

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output);

void ReadUsbStringDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<std::map<uint8_t, base::string16>> index_map,
    base::OnceCallback<void(std::unique_ptr<std::map<uint8_t, base::string16>>)>
        callback);

UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(const uint8_t* data);

UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(uint8_t address,
                                           uint8_t attributes,
                                           uint16_t maximum_packet_size,
                                           uint8_t polling_interval);

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_
