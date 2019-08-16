// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_win.h"

#include <Unknwn.h>
#include <WinDef.h>
#include <stdint.h>
#include <windows.h>

namespace device {

HidHapticGamepadWin::HidHapticGamepadWin(HANDLE device_handle,
                                         const HapticReportData& data)
    : HidHapticGamepadBase(data) {
  // Fetch the size of the RIDI_DEVICENAME string.
  UINT size;
  UINT result = ::GetRawInputDeviceInfo(device_handle, RIDI_DEVICENAME,
                                        /*pData=*/nullptr, &size);
  if (result == 0U) {
    // Read RIDI_DEVICENAME into a buffer.
    std::unique_ptr<wchar_t[]> name_buffer(new wchar_t[size]);
    result = ::GetRawInputDeviceInfo(device_handle, RIDI_DEVICENAME,
                                     name_buffer.get(), &size);
    if (result == size) {
      // Open the device handle for asynchronous I/O.
      hid_handle_.Set(::CreateFile(
          name_buffer.get(), GENERIC_READ | GENERIC_WRITE,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
          /*hTemplateFile=*/nullptr));
    }
  }
}

HidHapticGamepadWin::~HidHapticGamepadWin() = default;

// static
std::unique_ptr<HidHapticGamepadWin> HidHapticGamepadWin::Create(
    uint16_t vendor_id,
    uint16_t product_id,
    HANDLE device_handle) {
  const auto* haptic_data = GetHapticReportData(vendor_id, product_id);
  if (!haptic_data)
    return nullptr;
  return std::make_unique<HidHapticGamepadWin>(device_handle, *haptic_data);
}

void HidHapticGamepadWin::DoShutdown() {
  hid_handle_.Close();
}

size_t HidHapticGamepadWin::WriteOutputReport(
    base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  if (!hid_handle_.IsValid())
    return 0;

  base::win::ScopedHandle event_handle(
      ::CreateEvent(/*lpEventAttributes=*/nullptr,
                    /*bManualReset=*/false,
                    /*bInitialState=*/false,
                    /*lpName=*/L""));
  OVERLAPPED overlapped = {0};
  overlapped.hEvent = event_handle.Get();

  // Set up an asynchronous write.
  DWORD bytes_written = 0;
  BOOL write_success =
      ::WriteFile(hid_handle_.Get(), report.data(), report.size_bytes(),
                  &bytes_written, &overlapped);
  if (!write_success) {
    DWORD error = ::GetLastError();
    if (error == ERROR_IO_PENDING) {
      // Wait for the write to complete. This causes WriteOutputReport to behave
      // synchronously.
      DWORD wait_object = ::WaitForSingleObject(overlapped.hEvent,
                                                /*dwMilliseconds=*/100);
      if (wait_object == WAIT_OBJECT_0) {
        ::GetOverlappedResult(hid_handle_.Get(), &overlapped, &bytes_written,
                              /*bWait=*/true);
      } else {
        // Wait failed, or the timeout was exceeded before the write completed.
        // Cancel the write request.
        if (::CancelIo(hid_handle_.Get())) {
          HANDLE handles[2];
          handles[0] = hid_handle_.Get();
          handles[1] = overlapped.hEvent;
          ::WaitForMultipleObjects(/*nCount=*/2, handles,
                                   /*bWaitAll=*/false, INFINITE);
        }
      }
    }
  }
  return write_success ? bytes_written : 0;
}

base::WeakPtr<AbstractHapticGamepad> HidHapticGamepadWin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
