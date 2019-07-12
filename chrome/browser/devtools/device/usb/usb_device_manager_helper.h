// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

struct AndroidInterfaceInfo {
  AndroidInterfaceInfo(
      uint8_t interface_number,
      const device::mojom::UsbAlternateInterfaceInfo* alternate);

  uint8_t interface_number;
  const device::mojom::UsbAlternateInterfaceInfo* alternate;
};

struct AndroidDeviceInfo {
  AndroidDeviceInfo(const std::string& guid,
                    const std::string& serial,
                    int interface_id,
                    int inbound_address,
                    int outbound_address,
                    int zero_mask);
  AndroidDeviceInfo(const AndroidDeviceInfo& other);

  std::string guid;
  std::string serial;
  int interface_id = 0;
  int inbound_address = 0;
  int outbound_address = 0;
  int zero_mask = 0;
};

using AndroidDeviceInfoListCallback =
    base::OnceCallback<void(std::vector<AndroidDeviceInfo>)>;

// All methods in this class should be called in the single thread,
// handler_thread_ in AndroidDeviceManager.
class UsbDeviceManagerHelper {
 public:
  // This is prefered way to get UsbDeviceManagerHelper instance.
  static UsbDeviceManagerHelper* GetInstance();
  static void CountDevices(base::OnceCallback<void(int)> callback);
  static void SetUsbManagerForTesting(
      device::mojom::UsbDeviceManagerPtrInfo fake_usb_manager);

  // Please do not create UsbDeviceManagerHelper instance from this constructor
  // directly, use static method GetInstance() instead.
  UsbDeviceManagerHelper();
  virtual ~UsbDeviceManagerHelper();

  void GetAndroidDevices(AndroidDeviceInfoListCallback callback);

  void GetDevice(const std::string& guid,
                 device::mojom::UsbDeviceRequest device_request);

 private:
  void CountDevicesInternal(base::OnceCallback<void(int)> callback);
  void SetUsbManagerForTestingInternal(
      device::mojom::UsbDeviceManagerPtrInfo fake_usb_manager);
  void EnsureUsbDeviceManagerConnection();
  void OnDeviceManagerConnectionError();

  device::mojom::UsbDeviceManagerPtr device_manager_;
  // Just for test.
  device::mojom::UsbDeviceManagerPtrInfo testing_device_manager_info_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<UsbDeviceManagerHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceManagerHelper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_
