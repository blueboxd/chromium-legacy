// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_usb_device_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/public/cpp/test/fake_usb_device.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace device {

FakeUsbDeviceManager::FakeUsbDeviceManager() {}

FakeUsbDeviceManager::~FakeUsbDeviceManager() {}

void FakeUsbDeviceManager::EnumerateDevicesAndSetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
    EnumerateDevicesAndSetClientCallback callback) {
  GetDevices(nullptr, std::move(callback));
  SetClient(std::move(client));
}

// mojom::UsbDeviceManager implementation:
void FakeUsbDeviceManager::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                      GetDevicesCallback callback) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& it : devices_) {
    mojom::UsbDeviceInfoPtr device_info = it.second->GetDeviceInfo().Clone();
    if (UsbDeviceFilterMatchesAny(filters, *device_info)) {
      device_infos.push_back(std::move(device_info));
    }
  }

  std::move(callback).Run(std::move(device_infos));
}

void FakeUsbDeviceManager::GetDevice(const std::string& guid,
                                     mojom::UsbDeviceRequest device_request,
                                     mojom::UsbDeviceClientPtr device_client) {
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return;

  FakeUsbDevice::Create(it->second, std::move(device_request),
                        std::move(device_client));
}

#if defined(OS_ANDROID)
void FakeUsbDeviceManager::RefreshDeviceInfo(
    const std::string& guid,
    RefreshDeviceInfoCallback callback) {
  auto it = devices_.find(guid);
  if (it == devices_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(it->second->GetDeviceInfo().Clone());
}
#endif

#if defined(OS_CHROMEOS)
void FakeUsbDeviceManager::CheckAccess(const std::string& guid,
                                       CheckAccessCallback callback) {
  std::move(callback).Run(true);
}

void FakeUsbDeviceManager::OpenFileDescriptor(
    const std::string& guid,
    OpenFileDescriptorCallback callback) {
  std::move(callback).Run(base::File(
      base::FilePath(FILE_PATH_LITERAL("/dev/null")),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE));
}
#endif  // defined(OS_CHROMEOS)

void FakeUsbDeviceManager::SetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);
  mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void FakeUsbDeviceManager::AddBinding(mojom::UsbDeviceManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

mojom::UsbDeviceInfoPtr FakeUsbDeviceManager::AddDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(!base::Contains(devices_, device->guid()));
  devices_[device->guid()] = device;
  auto device_info = device->GetDeviceInfo().Clone();

  // Notify all the clients.
  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceAdded(device_info->Clone());
      });
  return device_info;
}

void FakeUsbDeviceManager::RemoveDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(base::Contains(devices_, device->guid()));

  auto device_info = device->GetDeviceInfo().Clone();
  devices_.erase(device->guid());

  // Notify all the clients
  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceRemoved(device_info->Clone());
      });

  device->NotifyDeviceRemoved();
}

void FakeUsbDeviceManager::RemoveDevice(const std::string& guid) {
  DCHECK(base::Contains(devices_, guid));

  RemoveDevice(devices_[guid]);
}

void FakeUsbDeviceManager::RemoveAllDevices() {
  std::vector<scoped_refptr<FakeUsbDeviceInfo>> device_list;
  for (const auto& pair : devices_) {
    device_list.push_back(pair.second);
  }
  for (const auto& device : device_list) {
    RemoveDevice(device);
  }
}

bool FakeUsbDeviceManager::SetMockForDevice(const std::string& guid,
                                            MockUsbMojoDevice* mock_device) {
  if (!base::Contains(devices_, guid))
    return false;

  devices_[guid]->SetMockDevice(mock_device);
  return true;
}

}  // namespace device
