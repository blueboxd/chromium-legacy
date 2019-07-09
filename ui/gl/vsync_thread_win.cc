// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win.h"

#include "base/bind.h"

namespace gl {
namespace {
Microsoft::WRL::ComPtr<IDXGIOutput> DXGIOutputFromMonitor(
    HMONITOR monitor,
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return nullptr;
  }

  size_t i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, &output)))
      break;

    DXGI_OUTPUT_DESC desc = {};
    if (FAILED(output->GetDesc(&desc))) {
      DLOG(ERROR) << "DXGI output GetDesc failed";
      return nullptr;
    }

    if (desc.Monitor == monitor)
      return output;
  }

  return nullptr;
}
}  // namespace

VSyncThreadWin::VSyncThreadWin(
    HWND window,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    VSyncCallback callback)
    : vsync_thread_("GpuVSyncThread"),
      window_(window),
      d3d11_device_(std::move(d3d11_device)),
      callback_(std::move(callback)) {
  DCHECK(window_);
  DCHECK(callback_);
  base::Thread::Options options;
  // Inherit priority from GPU main thread which depends on finch flags.
  options.priority = base::PlatformThread::GetCurrentThreadPriority();
  vsync_thread_.StartWithOptions(std::move(options));
}

VSyncThreadWin::~VSyncThreadWin() {
  SetEnabled(false);
  vsync_thread_.Stop();
}

void VSyncThreadWin::SetEnabled(bool enabled) {
  base::AutoLock auto_lock(lock_);
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled_ && !started_) {
    started_ = true;
    vsync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VSyncThreadWin::WaitForVSync, base::Unretained(this)));
  }
}

void VSyncThreadWin::WaitForVSync() {
  HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
  if (window_monitor_ != monitor) {
    window_monitor_ = monitor;
    window_output_ = DXGIOutputFromMonitor(monitor, d3d11_device_);
  }

  base::TimeDelta interval = base::TimeDelta::FromSecondsD(1.0 / 60);

  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(MONITORINFOEX);
  if (monitor && GetMonitorInfo(monitor, &monitor_info)) {
    DEVMODE display_info = {};
    display_info.dmSize = sizeof(DEVMODE);
    display_info.dmDriverExtra = 0;
    if (EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
                            &display_info) &&
        display_info.dmDisplayFrequency > 1) {
      interval =
          base::TimeDelta::FromSecondsD(1.0 / display_info.dmDisplayFrequency);
    }
  }

  base::TimeTicks wait_for_vblank_start_time = base::TimeTicks::Now();
  bool wait_for_vblank_succeeded =
      window_output_ && SUCCEEDED(window_output_->WaitForVBlank());

  // WaitForVBlank returns very early instead of waiting until vblank when the
  // monitor goes to sleep.  We use 1ms as a threshold for the duration of
  // WaitForVBlank and fallback to Sleep() if it returns before that.  This
  // could happen during normal operation for the first call after the vsync
  // callback is enabled, but it shouldn't happen often.
  const auto kVBlankIntervalThreshold = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta wait_for_vblank_elapsed_time =
      base::TimeTicks::Now() - wait_for_vblank_start_time;

  if (!wait_for_vblank_succeeded ||
      wait_for_vblank_elapsed_time < kVBlankIntervalThreshold) {
    Sleep(static_cast<DWORD>(interval.InMillisecondsRoundedUp()));
  }

  base::AutoLock auto_lock(lock_);
  DCHECK(started_);
  if (enabled_) {
    vsync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VSyncThreadWin::WaitForVSync, base::Unretained(this)));
    // Release lock before running callback to guard against any reentracny
    // deadlock.
    base::AutoUnlock auto_unlock(lock_);
    callback_.Run(base::TimeTicks::Now(), interval);
  } else {
    started_ = false;
  }
}

}  // namespace gl
