// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_

#include <windows.h>

// Must be after windows.h.
#include <hidclass.h>

// NOTE: <hidsdi.h> must be included beore <hidpi.h>. clang-format will want to
// reorder them.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "device/base/device_monitor_win.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/hid_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class HidServiceWin : public HidService, public DeviceMonitorWin::Observer {
 public:
  HidServiceWin();
  ~HidServiceWin() override;

  void Connect(const std::string& device_id, ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  static void EnumerateBlocking(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  static void CollectInfoFromButtonCaps(
      PHIDP_PREPARSED_DATA preparsed_data,
      HIDP_REPORT_TYPE report_type,
      USHORT button_caps_length,
      mojom::HidCollectionInfo* collection_info);
  static void CollectInfoFromValueCaps(
      PHIDP_PREPARSED_DATA preparsed_data,
      HIDP_REPORT_TYPE report_type,
      USHORT value_caps_length,
      mojom::HidCollectionInfo* collection_info);
  static void AddDeviceBlocking(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::string16& device_path);

  // DeviceMonitorWin::Observer implementation:
  void OnDeviceAdded(const GUID& class_guid,
                     const base::string16& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const base::string16& device_path) override;

  // Tries to open the device read-write and falls back to read-only.
  static base::win::ScopedHandle OpenDevice(const base::string16& device_path);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;
  base::WeakPtrFactory<HidServiceWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidServiceWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_
