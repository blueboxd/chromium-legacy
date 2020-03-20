// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/win/windows_types.h"
#include "device/base/device_monitor_win.h"
#include "services/device/serial/serial_device_enumerator.h"

typedef void* HDEVINFO;
typedef struct _SP_DEVINFO_DATA SP_DEVINFO_DATA;

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorWin : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorWin(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~SerialDeviceEnumeratorWin() override;

  // Searches for the COM port in the device's friendly name and returns the
  // appropriate device path or nullopt if the input did not contain a valid
  // name.
  static base::Optional<base::FilePath> GetPath(
      const std::string& friendly_name);

  void OnPathAdded(const base::string16& device_path);
  void OnPathRemoved(const base::string16& device_path);

 private:
  class UiThreadHelper;

  void DoInitialEnumeration();
  void EnumeratePort(HDEVINFO dev_info, SP_DEVINFO_DATA* dev_info_data);

  std::map<base::FilePath, base::UnguessableToken> paths_;

  std::unique_ptr<UiThreadHelper, base::OnTaskRunnerDeleter> helper_;
  base::WeakPtrFactory<SerialDeviceEnumeratorWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SerialDeviceEnumeratorWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
