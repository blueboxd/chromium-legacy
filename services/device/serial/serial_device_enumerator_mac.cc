// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_mac.h"

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%08x", error_code);
}

// Searches a service and all ancestor services for a property with the
// specified key, returning NULL if no such key was found.
CFTypeRef GetCFProperty(io_service_t service, const CFStringRef key) {
  // We search for the specified property not only on the specified service, but
  // all ancestors of that service. This is important because if a device is
  // both serial and USB, in the registry tree it appears as a serial service
  // with a USB service as its ancestor. Without searching ancestors services
  // for the specified property, we'd miss all USB properties.
  return IORegistryEntrySearchCFProperty(
      service, kIOServicePlane, key, NULL,
      kIORegistryIterateRecursively | kIORegistryIterateParents);
}

// Searches a service and all ancestor services for a string property with the
// specified key, returning NULL if no such key was found.
CFStringRef GetCFStringProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFStringGetTypeID()))
    return static_cast<CFStringRef>(value);

  return NULL;
}

// Searches a service and all ancestor services for a number property with the
// specified key, returning NULL if no such key was found.
CFNumberRef GetCFNumberProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFNumberGetTypeID()))
    return static_cast<CFNumberRef>(value);

  return NULL;
}

// Searches the specified service for a string property with the specified key.
base::Optional<std::string> GetStringProperty(io_service_t service,
                                              const CFStringRef key) {
  CFStringRef propValue = GetCFStringProperty(service, key);
  if (propValue)
    return base::SysCFStringRefToUTF8(propValue);

  return base::nullopt;
}

// Searches the specified service for a uint16_t property with the specified
// key.
base::Optional<uint16_t> GetUInt16Property(io_service_t service,
                                           const CFStringRef key) {
  CFNumberRef propValue = GetCFNumberProperty(service, key);
  if (propValue) {
    int intValue;
    if (CFNumberGetValue(propValue, kCFNumberIntType, &intValue))
      return static_cast<uint16_t>(intValue);
  }

  return base::nullopt;
}

}  // namespace

SerialDeviceEnumeratorMac::SerialDeviceEnumeratorMac() {
  notify_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  CFRunLoopAddSource(CFRunLoopGetMain(),
                     IONotificationPortGetRunLoopSource(notify_port_.get()),
                     kCFRunLoopDefaultMode);

  IOReturn result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOFirstMatchNotification,
      IOServiceMatching(kIOSerialBSDServiceValue), FirstMatchCallback, this,
      devices_added_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    DLOG(ERROR) << "Failed to listen for device arrival: "
                << HexErrorCode(result);
    return;
  }

  // Drain |devices_added_iterator_| to arm the notification.
  AddDevices();

  result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOTerminatedNotification,
      IOServiceMatching(kIOSerialBSDServiceValue), TerminatedCallback, this,
      devices_removed_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    DLOG(ERROR) << "Failed to listen for device removal: "
                << HexErrorCode(result);
    return;
  }

  // Drain |devices_removed_iterator_| to arm the notification.
  RemoveDevices();
}

SerialDeviceEnumeratorMac::~SerialDeviceEnumeratorMac() = default;

// static
void SerialDeviceEnumeratorMac::FirstMatchCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_added_iterator_, iterator);
  enumerator->AddDevices();
}

// static
void SerialDeviceEnumeratorMac::TerminatedCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_removed_iterator_, iterator);
  enumerator->RemoveDevices();
}

void SerialDeviceEnumeratorMac::AddDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_)), device) {
    uint64_t entry_id;
    IOReturn result = IORegistryEntryGetRegistryEntryID(device, &entry_id);
    if (result != kIOReturnSuccess)
      continue;

    auto info = mojom::SerialPortInfo::New();
    base::Optional<uint16_t> vendor_id =
        GetUInt16Property(device.get(), CFSTR(kUSBVendorID));
    if (vendor_id) {
      info->has_vendor_id = true;
      info->vendor_id = *vendor_id;
    }

    base::Optional<uint16_t> product_id =
        GetUInt16Property(device.get(), CFSTR(kUSBProductID));
    if (product_id) {
      info->has_product_id = true;
      info->product_id = *product_id;
    }

    info->display_name =
        GetStringProperty(device.get(), CFSTR(kUSBProductString));

    base::Optional<std::string> serial_number =
        GetStringProperty(device.get(), CFSTR(kUSBSerialNumberString));
    if (vendor_id && product_id && serial_number) {
      info->persistent_id = base::StringPrintf(
          "%04X-%04X-%s", *vendor_id, *product_id, serial_number->c_str());
    }

    // Each serial device has two paths associated with it: a "dialin" path
    // starting with "tty" and a "callout" path starting with "cu". The
    // call-out device is typically preferred but requesting the dial-in device
    // is supported for the legacy Chrome Apps API.
    base::Optional<std::string> dialin_device =
        GetStringProperty(device.get(), CFSTR(kIODialinDeviceKey));
    base::Optional<std::string> callout_device =
        GetStringProperty(device.get(), CFSTR(kIOCalloutDeviceKey));

    if (callout_device) {
      info->path = base::FilePath(*callout_device);
      if (dialin_device)
        info->alternate_path = base::FilePath(*dialin_device);
    } else if (dialin_device) {
      info->path = base::FilePath(*dialin_device);
    } else {
      continue;
    }

    auto token = base::UnguessableToken::Create();
    info->token = token;

    entries_.insert({entry_id, token});
    AddPort(std::move(info));
  }
}

void SerialDeviceEnumeratorMac::RemoveDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_)), device) {
    uint64_t entry_id;
    IOReturn result = IORegistryEntryGetRegistryEntryID(device, &entry_id);
    if (result != kIOReturnSuccess)
      continue;

    auto it = entries_.find(entry_id);
    if (it == entries_.end())
      continue;

    base::UnguessableToken token = it->second;
    entries_.erase(it);

    RemovePort(token);
  }
}

}  // namespace device
