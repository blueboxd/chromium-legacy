// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <devguid.h>
#include <ntddser.h>
#include <setupapi.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "base/scoped_generic.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "third_party/re2/src/re2/re2.h"

namespace device {

namespace {

struct DevInfoScopedTraits {
  static HDEVINFO InvalidValue() { return INVALID_HANDLE_VALUE; }
  static void Free(HDEVINFO h) { SetupDiDestroyDeviceInfoList(h); }
};

using ScopedDevInfo = base::ScopedGeneric<HDEVINFO, DevInfoScopedTraits>;

// Searches the specified device info for a property with the specified key,
// assigns the result to value, and returns whether the operation was
// successful.
bool GetProperty(HDEVINFO dev_info,
                 SP_DEVINFO_DATA* dev_info_data,
                 const int key,
                 std::string* value) {
  // We don't know how much space the property's value will take up, so we call
  // the property retrieval function once to fetch the size of the required
  // value buffer, then again once we've allocated a sufficiently large buffer.
  DWORD buffer_size = 0;
  SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, key, nullptr,
                                   nullptr, buffer_size, &buffer_size);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  base::string16 buffer;
  if (!SetupDiGetDeviceRegistryProperty(
          dev_info, dev_info_data, key, nullptr,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, buffer_size)),
          buffer_size, nullptr)) {
    return false;
  }

  *value = base::UTF16ToUTF8(buffer);
  return true;
}

base::FilePath FixUpPortName(base::StringPiece port_name) {
  // For COM numbers less than 9, CreateFile is called with a string such as
  // "COM1". For numbers greater than 9, a prefix of "\\.\" must be added.
  if (port_name.length() > std::string("COM9").length())
    return base::FilePath(LR"(\\.\)").AppendASCII(port_name);

  return base::FilePath::FromUTF8Unsafe(port_name);
}

// Searches for the display name in the device's friendly name, assigns its
// value to display_name, and returns whether the operation was successful.
bool GetDisplayName(const std::string friendly_name,
                    std::string* display_name) {
  return RE2::PartialMatch(friendly_name, R"((.*) \(COM[0-9]+\))",
                           display_name);
}

// Searches for the vendor ID in the device's hardware ID, assigns its value to
// vendor_id, and returns whether the operation was successful.
bool GetVendorID(const std::string hardware_id, uint32_t* vendor_id) {
  std::string vendor_id_str;
  return RE2::PartialMatch(hardware_id, "VID_([0-9a-fA-F]+)", &vendor_id_str) &&
         base::HexStringToUInt(vendor_id_str, vendor_id);
}

// Searches for the product ID in the device's product ID, assigns its value to
// product_id, and returns whether the operation was successful.
bool GetProductID(const std::string hardware_id, uint32_t* product_id) {
  std::string product_id_str;
  return RE2::PartialMatch(hardware_id, "PID_([0-9a-fA-F]+)",
                           &product_id_str) &&
         base::HexStringToUInt(product_id_str, product_id);
}

}  // namespace

class SerialDeviceEnumeratorWin::UiThreadHelper
    : public DeviceMonitorWin::Observer {
 public:
  UiThreadHelper() : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Disallow copy and assignment.
  UiThreadHelper(UiThreadHelper&) = delete;
  UiThreadHelper& operator=(UiThreadHelper&) = delete;

  virtual ~UiThreadHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Initialize(base::WeakPtr<SerialDeviceEnumeratorWin> enumerator) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    enumerator_ = std::move(enumerator);
    device_observer_.Add(
        DeviceMonitorWin::GetForDeviceInterface(GUID_DEVINTERFACE_COMPORT));
  }

  void OnDeviceAdded(const GUID& class_guid,
                     const base::string16& device_path) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialDeviceEnumeratorWin::OnPathAdded,
                                  enumerator_, device_path));
  }

  void OnDeviceRemoved(const GUID& class_guid,
                       const base::string16& device_path) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialDeviceEnumeratorWin::OnPathRemoved,
                                  enumerator_, device_path));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak reference to the SerialDeviceEnumeratorWin that owns this object.
  // Calls on |enumerator_| must be posted to |task_runner_|.
  base::WeakPtr<SerialDeviceEnumeratorWin> enumerator_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_{
      this};
};

SerialDeviceEnumeratorWin::SerialDeviceEnumeratorWin(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : helper_(new UiThreadHelper(), base::OnTaskRunnerDeleter(ui_task_runner)) {
  // Passing a raw pointer to |helper_| is safe here because this task will
  // reach the UI thread before any task to delete |helper_|.
  ui_task_runner->PostTask(FROM_HERE,
                           base::BindOnce(&UiThreadHelper::Initialize,
                                          base::Unretained(helper_.get()),
                                          weak_factory_.GetWeakPtr()));

  DoInitialEnumeration();
}

SerialDeviceEnumeratorWin::~SerialDeviceEnumeratorWin() = default;

// static
base::Optional<base::FilePath> SerialDeviceEnumeratorWin::GetPath(
    const std::string& friendly_name) {
  std::string com_port;
  if (!RE2::PartialMatch(friendly_name, ".* \\((COM[0-9]+)\\)", &com_port))
    return base::nullopt;

  return FixUpPortName(com_port);
}

void SerialDeviceEnumeratorWin::OnPathAdded(const base::string16& device_path) {
  ScopedDevInfo dev_info(SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid())
    return;

  if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                  nullptr)) {
    return;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiEnumDeviceInfo(dev_info.get(), 0, &dev_info_data))
    return;

  EnumeratePort(dev_info.get(), &dev_info_data);
}

void SerialDeviceEnumeratorWin::OnPathRemoved(
    const base::string16& device_path) {
  ScopedDevInfo dev_info(SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid())
    return;

  if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                  nullptr)) {
    return;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiEnumDeviceInfo(dev_info.get(), 0, &dev_info_data))
    return;

  std::string friendly_name;
  // SPDRP_FRIENDLYNAME looks like "USB_SERIAL_PORT (COM3)".
  // In Windows, the COM port is the path used to uniquely identify the
  // serial device. If the COM can't be found, ignore the device.
  if (!GetProperty(dev_info.get(), &dev_info_data, SPDRP_FRIENDLYNAME,
                   &friendly_name)) {
    return;
  }

  base::Optional<base::FilePath> path = GetPath(friendly_name);
  if (!path)
    return;

  auto it = paths_.find(*path);
  if (it == paths_.end())
    return;

  base::UnguessableToken token = it->second;

  paths_.erase(it);
  RemovePort(token);
}

void SerialDeviceEnumeratorWin::DoInitialEnumeration() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Make a device interface query to find all serial devices.
  ScopedDevInfo dev_info(
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, 0,
                          DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
  if (!dev_info.is_valid())
    return;

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info.get(), i, &dev_info_data);
       i++) {
    EnumeratePort(dev_info.get(), &dev_info_data);
  }
}

void SerialDeviceEnumeratorWin::EnumeratePort(HDEVINFO dev_info,
                                              SP_DEVINFO_DATA* dev_info_data) {
  std::string friendly_name;
  // SPDRP_FRIENDLYNAME looks like "USB_SERIAL_PORT (COM3)".
  // In Windows, the COM port is the path used to uniquely identify the
  // serial device. If the COM can't be found, ignore the device.
  if (!GetProperty(dev_info, dev_info_data, SPDRP_FRIENDLYNAME,
                   &friendly_name)) {
    return;
  }

  base::Optional<base::FilePath> path = GetPath(friendly_name);
  if (!path)
    return;

  auto info = mojom::SerialPortInfo::New();
  info->path = *path;
  base::UnguessableToken token = base::UnguessableToken::Create();
  info->token = token;

  std::string display_name;
  if (GetDisplayName(friendly_name, &display_name))
    info->display_name = std::move(display_name);

  std::string hardware_id;
  // SPDRP_HARDWAREID looks like "FTDIBUS\COMPORT&VID_0403&PID_6001".
  if (GetProperty(dev_info, dev_info_data, SPDRP_HARDWAREID, &hardware_id)) {
    uint32_t vendor_id, product_id;
    if (GetVendorID(hardware_id, &vendor_id)) {
      info->has_vendor_id = true;
      info->vendor_id = vendor_id;
    }
    if (GetProductID(hardware_id, &product_id)) {
      info->has_product_id = true;
      info->product_id = product_id;
    }
  }

  paths_.insert(std::make_pair(*path, token));
  AddPort(std::move(info));
}

}  // namespace device
