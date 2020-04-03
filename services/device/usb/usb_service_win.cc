// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_win.h"

#include <objbase.h>
#include <setupapi.h>
#include <stdint.h>
#include <usbiodef.h>

#define INITGUID
#include <devpkey.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_generic.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

namespace {

struct DevInfoScopedTraits {
  static HDEVINFO InvalidValue() { return INVALID_HANDLE_VALUE; }
  static void Free(HDEVINFO h) { SetupDiDestroyDeviceInfoList(h); }
};

using ScopedDevInfo = base::ScopedGeneric<HDEVINFO, DevInfoScopedTraits>;

bool GetDeviceUint32Property(HDEVINFO dev_info,
                             SP_DEVINFO_DATA* dev_info_data,
                             const DEVPROPKEY& property,
                             uint32_t* property_buffer) {
  DEVPROPTYPE property_type;
  if (!SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                                &property_type,
                                reinterpret_cast<PBYTE>(property_buffer),
                                sizeof(*property_buffer), nullptr, 0) ||
      property_type != DEVPROP_TYPE_UINT32) {
    return false;
  }

  return true;
}

bool GetDeviceStringProperty(HDEVINFO dev_info,
                             SP_DEVINFO_DATA* dev_info_data,
                             const DEVPROPKEY& property,
                             base::string16* buffer) {
  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, nullptr, 0, &required_size, 0) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      property_type != DEVPROP_TYPE_STRING) {
    return false;
  }

  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(buffer, required_size)),
          required_size, nullptr, 0)) {
    return false;
  }

  return true;
}

bool GetDeviceStringListProperty(HDEVINFO dev_info,
                                 SP_DEVINFO_DATA* dev_info_data,
                                 const DEVPROPKEY& property,
                                 std::vector<base::string16>* result) {
  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, nullptr, 0, &required_size, 0) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      property_type != DEVPROP_TYPE_STRING_LIST) {
    return false;
  }

  base::string16 buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, nullptr, 0)) {
    return false;
  }

  // Windows string list properties use a NUL character as the delimiter.
  *result = base::SplitString(buffer, base::StringPiece16(L"\0", 1),
                              base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return true;
}

bool GetServiceName(HDEVINFO dev_info,
                    SP_DEVINFO_DATA* dev_info_data,
                    base::string16* service_name) {
  base::string16 buffer;
  if (!GetDeviceStringProperty(dev_info, dev_info_data, DEVPKEY_Device_Service,
                               &buffer)) {
    return false;
  }

  // Windows pads this string with a variable number of NUL bytes for no
  // discernible reason.
  *service_name = base::TrimString(buffer, base::StringPiece16(L"\0", 1),
                                   base::TRIM_TRAILING)
                      .as_string();
  return true;
}

bool GetDeviceInterfaceDetails(HDEVINFO dev_info,
                               SP_DEVICE_INTERFACE_DATA* device_interface_data,
                               base::string16* device_path,
                               uint32_t* bus_number,
                               uint32_t* port_number,
                               base::string16* parent_instance_id,
                               std::vector<base::string16>* child_instance_ids,
                               base::string16* service_name) {
  DWORD required_size = 0;
  if (SetupDiGetDeviceInterfaceDetail(dev_info, device_interface_data, nullptr,
                                      0, &required_size, nullptr) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }

  std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
  device_interface_detail_data(
      static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
  device_interface_detail_data->cbSize = sizeof(*device_interface_detail_data);

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);

  if (!SetupDiGetDeviceInterfaceDetail(
          dev_info, device_interface_data, device_interface_detail_data.get(),
          required_size, nullptr, &dev_info_data)) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceInterfaceDetail";
    return false;
  }

  if (device_path) {
    *device_path = base::string16(device_interface_detail_data->DevicePath);
  }

  if (bus_number) {
    if (!GetDeviceUint32Property(dev_info, &dev_info_data,
                                 DEVPKEY_Device_BusNumber, bus_number)) {
      USB_PLOG(ERROR) << "Failed to get device bus number";
      return false;
    }
  }

  if (port_number) {
    if (!GetDeviceUint32Property(dev_info, &dev_info_data,
                                 DEVPKEY_Device_Address, port_number)) {
      USB_PLOG(ERROR) << "Failed to get device address";
      return false;
    }
  }

  if (parent_instance_id) {
    if (!GetDeviceStringProperty(dev_info, &dev_info_data,
                                 DEVPKEY_Device_Parent, parent_instance_id)) {
      USB_PLOG(ERROR) << "Failed to get the device parent";
      return false;
    }
  }

  if (child_instance_ids) {
    if (!GetDeviceStringListProperty(dev_info, &dev_info_data,
                                     DEVPKEY_Device_Children,
                                     child_instance_ids) &&
        GetLastError() != ERROR_NOT_FOUND) {
      USB_PLOG(ERROR) << "Failed to get device children";
      return false;
    }
  }

  if (service_name) {
    if (!GetServiceName(dev_info, &dev_info_data, service_name)) {
      USB_PLOG(ERROR) << "Failed to get device driver name";
      return false;
    }
  }

  return true;
}

bool GetDevicePath(const base::string16& instance_id,
                   const GUID& device_interface_guid,
                   base::string16* device_path) {
  ScopedDevInfo dev_info(
      SetupDiGetClassDevs(&device_interface_guid, instance_id.c_str(), 0,
                          DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
  if (!dev_info.is_valid()) {
    USB_PLOG(ERROR) << "SetupDiGetClassDevs";
    return false;
  }

  SP_DEVICE_INTERFACE_DATA device_interface_data = {};
  device_interface_data.cbSize = sizeof(device_interface_data);
  if (!SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                   &device_interface_guid, 0,
                                   &device_interface_data)) {
    USB_PLOG(ERROR) << "SetupDiEnumDeviceInterfaces";
    return false;
  }

  return GetDeviceInterfaceDetails(dev_info.get(), &device_interface_data,
                                   device_path, nullptr, nullptr, nullptr,
                                   nullptr, nullptr);
}

bool GetWinUsbDevicePath(const base::string16& instance_id,
                         base::string16* device_path) {
  ScopedDevInfo dev_info(SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid()) {
    USB_PLOG(ERROR) << "SetupDiCreateDeviceInfoList";
    return false;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiOpenDeviceInfo(dev_info.get(), instance_id.c_str(), nullptr, 0,
                             &dev_info_data)) {
    USB_PLOG(ERROR) << "SetupDiOpenDeviceInfo";
    return false;
  }

  base::string16 service_name;
  if (!GetServiceName(dev_info.get(), &dev_info_data, &service_name)) {
    USB_PLOG(ERROR) << "Could not get child device's service name";
    return false;
  }

  if (!base::EqualsCaseInsensitiveASCII(service_name, L"winusb"))
    return false;

  // There is no standard device interface GUID for USB functions and so we
  // must discover the set of GUIDs that have been set in the registry by
  // the INF file or Microsoft OS Compatibility descriptors before
  // SetupDiGetDeviceInterfaceDetail() can be used to get the device path.
  HKEY key = SetupDiOpenDevRegKey(dev_info.get(), &dev_info_data,
                                  DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if (key == INVALID_HANDLE_VALUE) {
    USB_PLOG(ERROR) << "Could not open device registry key";
    return false;
  }
  base::win::RegKey scoped_key(key);

  std::vector<std::wstring> device_interface_guids;
  LONG result =
      scoped_key.ReadValues(L"DeviceInterfaceGUIDs", &device_interface_guids);
  if (result != ERROR_SUCCESS) {
    USB_LOG(ERROR) << "Could not read device interface GUIDs: "
                   << logging::SystemErrorCodeToString(result);
    return false;
  }

  for (const auto& guid_string : device_interface_guids) {
    GUID guid;
    if (FAILED(CLSIDFromString(guid_string.c_str(), &guid))) {
      USB_LOG(ERROR) << "Failed to parse device interface GUID: "
                     << guid_string;
      continue;
    }

    if (GetDevicePath(instance_id, guid, device_path))
      return true;
  }

  return false;
}

}  // namespace

class UsbServiceWin::BlockingTaskRunnerHelper {
 public:
  explicit BlockingTaskRunnerHelper(base::WeakPtr<UsbServiceWin> service)
      : service_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        service_(service) {}
  ~BlockingTaskRunnerHelper() {}

  void EnumerateDevices() {
    ScopedDevInfo dev_info(
        SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, 0,
                            DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
    if (!dev_info.is_valid()) {
      USB_PLOG(ERROR) << "Failed to set up device enumeration";
      service_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbServiceWin::HelperStarted, service_));
      return;
    }

    SP_DEVICE_INTERFACE_DATA device_interface_data = {};
    device_interface_data.cbSize = sizeof(device_interface_data);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                                  &GUID_DEVINTERFACE_USB_DEVICE,
                                                  i, &device_interface_data);
         ++i) {
      EnumerateDevice(dev_info.get(), &device_interface_data, base::nullopt);
    }

    if (GetLastError() != ERROR_NO_MORE_ITEMS)
      USB_PLOG(ERROR) << "Failed to enumerate devices";

    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbServiceWin::HelperStarted, service_));
  }

  void EnumerateDevicePath(const base::string16& device_path) {
    ScopedDevInfo dev_info(
        SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, 0,
                            DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
    if (!dev_info.is_valid()) {
      USB_PLOG(ERROR) << "Failed to set up device enumeration";
      return;
    }

    SP_DEVICE_INTERFACE_DATA device_interface_data = {};
    device_interface_data.cbSize = sizeof(device_interface_data);
    if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                    &device_interface_data)) {
      USB_PLOG(ERROR) << "Failed to add device interface: " << device_path;
      return;
    }

    EnumerateDevice(dev_info.get(), &device_interface_data, device_path);
  }

  void EnumerateDevice(HDEVINFO dev_info,
                       SP_DEVICE_INTERFACE_DATA* device_interface_data,
                       const base::Optional<base::string16>& opt_device_path) {
    base::string16 device_path;
    base::string16* device_path_ptr = &device_path;
    if (opt_device_path) {
      device_path = *opt_device_path;
      device_path_ptr = nullptr;
    }

    uint32_t bus_number;
    uint32_t port_number;
    base::string16 parent_instance_id;
    std::vector<base::string16> child_instance_ids;
    base::string16 service_name;
    if (!GetDeviceInterfaceDetails(dev_info, device_interface_data,
                                   device_path_ptr, &bus_number, &port_number,
                                   &parent_instance_id, &child_instance_ids,
                                   &service_name)) {
      return;
    }

    // For composite devices Windows loads the usbccgp driver, which creates
    // child device notes for each of the device functions. It is these device
    // paths for these children which must be opened in order to communicate
    // with the WinUSB driver.
    std::vector<base::string16> child_device_paths;
    if (base::EqualsCaseInsensitiveASCII(service_name, L"usbccgp")) {
      for (const base::string16& instance_id : child_instance_ids) {
        base::string16 child_device_path;
        if (GetWinUsbDevicePath(instance_id, &child_device_path))
          child_device_paths.push_back(std::move(child_device_path));
      }
    }

    base::string16& hub_path = hub_paths_[parent_instance_id];
    if (hub_path.empty()) {
      base::string16 parent_path;
      if (!GetDevicePath(parent_instance_id, GUID_DEVINTERFACE_USB_HUB,
                         &parent_path)) {
        return;
      }
      hub_path = parent_path;
    }

    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbServiceWin::CreateDeviceObject, service_,
                                  device_path, hub_path, child_device_paths,
                                  bus_number, port_number, service_name));
  }

 private:
  std::unordered_map<base::string16, base::string16> hub_paths_;

  // Calls back to |service_| must be posted to |service_task_runner_|, which
  // runs tasks on the thread where that object lives.
  scoped_refptr<base::SingleThreadTaskRunner> service_task_runner_;
  base::WeakPtr<UsbServiceWin> service_;
};

UsbServiceWin::UsbServiceWin()
    : UsbService(),
      blocking_task_runner_(CreateBlockingTaskRunner()),
      helper_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)),
      device_observer_(this) {
  DeviceMonitorWin* device_monitor =
      DeviceMonitorWin::GetForDeviceInterface(GUID_DEVINTERFACE_USB_DEVICE);
  if (device_monitor)
    device_observer_.Add(device_monitor);

  helper_.reset(new BlockingTaskRunnerHelper(weak_factory_.GetWeakPtr()));
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::EnumerateDevices,
                                base::Unretained(helper_.get())));
}

UsbServiceWin::~UsbServiceWin() {
  NotifyWillDestroyUsbService();
}

void UsbServiceWin::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enumeration_ready())
    UsbService::GetDevices(std::move(callback));
  else
    enumeration_callbacks_.push_back(std::move(callback));
}

void UsbServiceWin::OnDeviceAdded(const GUID& class_guid,
                                  const base::string16& device_path) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::EnumerateDevicePath,
                                base::Unretained(helper_.get()), device_path));
}

void UsbServiceWin::OnDeviceRemoved(const GUID& class_guid,
                                    const base::string16& device_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto by_path_it = devices_by_path_.find(device_path);
  if (by_path_it == devices_by_path_.end())
    return;

  scoped_refptr<UsbDeviceWin> device = by_path_it->second;
  devices_by_path_.erase(by_path_it);
  device->OnDisconnect();

  auto by_guid_it = devices().find(device->guid());
  if (by_guid_it != devices().end() && enumeration_ready()) {
    USB_LOG(USER) << "USB device removed: path=" << device->device_path()
                  << " guid=" << device->guid();

    devices().erase(by_guid_it);
    NotifyDeviceRemoved(device);
  }
}

void UsbServiceWin::HelperStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_started_ = true;
  if (enumeration_ready()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  }
}

void UsbServiceWin::CreateDeviceObject(
    const base::string16& device_path,
    const base::string16& hub_path,
    const std::vector<base::string16>& child_device_paths,
    uint32_t bus_number,
    uint32_t port_number,
    const base::string16& driver_name) {
  // Devices that appear during initial enumeration are gathered into the first
  // result returned by GetDevices() and prevent device add/remove notifications
  // from being sent.
  if (!enumeration_ready())
    ++first_enumeration_countdown_;

  auto device = base::MakeRefCounted<UsbDeviceWin>(
      device_path, hub_path, child_device_paths, bus_number, port_number,
      driver_name);
  devices_by_path_[device->device_path()] = device;
  device->ReadDescriptors(base::BindOnce(&UsbServiceWin::DeviceReady,
                                         weak_factory_.GetWeakPtr(), device));
}

void UsbServiceWin::DeviceReady(scoped_refptr<UsbDeviceWin> device,
                                bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool enumeration_became_ready = false;
  if (!enumeration_ready()) {
    DCHECK_GT(first_enumeration_countdown_, 0u);
    first_enumeration_countdown_--;
    if (enumeration_ready())
      enumeration_became_ready = true;
  }

  // If |device| was disconnected while descriptors were being read then it
  // will have been removed from |devices_by_path_|.
  auto it = devices_by_path_.find(device->device_path());
  if (it == devices_by_path_.end()) {
    success = false;
  } else if (success) {
    DCHECK(!base::Contains(devices(), device->guid()));
    devices()[device->guid()] = device;

    USB_LOG(USER) << "USB device added: path=" << device->device_path()
                  << " vendor=" << device->vendor_id() << " \""
                  << device->manufacturer_string()
                  << "\", product=" << device->product_id() << " \""
                  << device->product_string() << "\", serial=\""
                  << device->serial_number() << "\", driver=\""
                  << device->driver_name() << "\", children=["
                  << base::JoinString(device->child_device_paths(), L", ")
                  << "], guid=" << device->guid();
  } else {
    devices_by_path_.erase(it);
  }

  if (enumeration_became_ready) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  } else if (success && enumeration_ready()) {
    NotifyDeviceAdded(device);
  }
}

}  // namespace device
