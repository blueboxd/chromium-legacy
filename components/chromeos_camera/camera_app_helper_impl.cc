// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/camera_app_helper_impl.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"

namespace chromeos_camera {

CameraAppHelperImpl::CameraAppHelperImpl(
    CameraResultCallback camera_result_callback,
    aura::Window* window)
    : camera_result_callback_(std::move(camera_result_callback)) {
  DCHECK(window);
  window->SetProperty(ash::kCanConsumeSystemKeysKey, true);
  ash::TabletMode::Get()->AddObserver(this);
}

CameraAppHelperImpl::~CameraAppHelperImpl() {
  ash::TabletMode::Get()->RemoveObserver(this);
}

void CameraAppHelperImpl::HandleCameraResult(
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    HandleCameraResultCallback callback) {
  camera_result_callback_.Run(intent_id, action, data, std::move(callback));
}

void CameraAppHelperImpl::IsTabletMode(IsTabletModeCallback callback) {
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::StartPerfEventTrace(const std::string& event) {
  TRACE_EVENT_BEGIN0("camera", event.c_str());
}

void CameraAppHelperImpl::StopPerfEventTrace(const std::string& event) {
  TRACE_EVENT_END0("camera", event.c_str());
}

void CameraAppHelperImpl::SetTabletMonitor(
    mojo::PendingRemote<TabletModeMonitor> monitor,
    SetTabletMonitorCallback callback) {
  monitor_ = mojo::Remote<TabletModeMonitor>(std::move(monitor));
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::OnTabletModeStarted() {
  if (monitor_.is_bound())
    monitor_->Update(true);
}

void CameraAppHelperImpl::OnTabletModeEnded() {
  if (monitor_.is_bound())
    monitor_->Update(false);
}

}  // namespace chromeos_camera
