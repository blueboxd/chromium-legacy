// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/usb/usb_service.h"

namespace device {

class UsbDevice;

namespace usb {

// Implements the public Mojo UsbDeviceManager interface by wrapping the
// UsbService instance.
class DeviceManagerImpl : public mojom::UsbDeviceManager,
                          public UsbService::Observer {
 public:
  DeviceManagerImpl();
  // Mostly be used for testing.
  explicit DeviceManagerImpl(std::unique_ptr<UsbService> usb_service);
  ~DeviceManagerImpl() override;

  void AddBinding(mojom::UsbDeviceManagerRequest request);

  UsbService* GetUsbService() const { return usb_service_.get(); }

 private:
  // DeviceManager implementation:
  void EnumerateDevicesAndSetClient(
      mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
      EnumerateDevicesAndSetClientCallback callback) override;
  void GetDevices(mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(const std::string& guid,
                 mojom::UsbDeviceRequest device_request,
                 mojom::UsbDeviceClientPtr device_client) override;

#if defined(OS_ANDROID)
  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override;
  void OnPermissionGrantedToRefresh(scoped_refptr<UsbDevice> device,
                                    RefreshDeviceInfoCallback callback,
                                    bool granted);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  void CheckAccess(const std::string& guid,
                   CheckAccessCallback callback) override;

  void OpenFileDescriptor(const std::string& guid,
                          OpenFileDescriptorCallback callback) override;

  void OnOpenFileDescriptor(OpenFileDescriptorCallback callback,
                            base::ScopedFD fd);

  void OnOpenFileDescriptorError(OpenFileDescriptorCallback callback,
                                 const std::string& error_name,
                                 const std::string& message);
#endif  // defined(OS_CHROMEOS)

  void SetClient(
      mojom::UsbDeviceManagerClientAssociatedPtrInfo client) override;

  // Callbacks to handle the async responses from the underlying UsbService.
  void OnGetDevices(mojom::UsbEnumerationOptionsPtr options,
                    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
                    GetDevicesCallback callback,
                    const std::vector<scoped_refptr<UsbDevice>>& devices);

  // UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override;
  void WillDestroyUsbService() override;

  void MaybeRunDeviceChangesCallback();

  std::unique_ptr<UsbService> usb_service_;
  ScopedObserver<UsbService, UsbService::Observer> observer_;

  mojo::BindingSet<mojom::UsbDeviceManager> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::UsbDeviceManagerClient> clients_;

  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerImpl);
};

}  // namespace usb
}  // namespace device

#endif  // SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
