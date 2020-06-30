// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_helper_impl.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"

namespace chromeos_camera {
namespace {

mojom::ScreenState ToMojoScreenState(ash::ScreenState s) {
  switch (s) {
    case ash::ScreenState::ON:
      return mojom::ScreenState::ON;
    case ash::ScreenState::OFF:
      return mojom::ScreenState::OFF;
    case ash::ScreenState::OFF_AUTO:
      return mojom::ScreenState::OFF_AUTO;
    default:
      NOTREACHED();
  }
}

}  // namespace

CameraAppHelperImpl::CameraAppHelperImpl(
    CameraResultCallback camera_result_callback,
    aura::Window* window)
    : camera_result_callback_(std::move(camera_result_callback)) {
  DCHECK(window);
  window->SetProperty(ash::kCanConsumeSystemKeysKey, true);
  ash::TabletMode::Get()->AddObserver(this);
  ash::ScreenBacklight::Get()->AddObserver(this);
}

CameraAppHelperImpl::~CameraAppHelperImpl() {
  ash::TabletMode::Get()->RemoveObserver(this);
  ash::ScreenBacklight::Get()->RemoveObserver(this);
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
  tablet_monitor_ = mojo::Remote<TabletModeMonitor>(std::move(monitor));
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::SetScreenStateMonitor(
    mojo::PendingRemote<ScreenStateMonitor> monitor,
    SetScreenStateMonitorCallback callback) {
  screen_state_monitor_ = mojo::Remote<ScreenStateMonitor>(std::move(monitor));
  auto&& mojo_state =
      ToMojoScreenState(ash::ScreenBacklight::Get()->GetScreenState());
  std::move(callback).Run(mojo_state);
}

void CameraAppHelperImpl::OnTabletModeStarted() {
  if (tablet_monitor_.is_bound())
    tablet_monitor_->Update(true);
}

void CameraAppHelperImpl::OnTabletModeEnded() {
  if (tablet_monitor_.is_bound())
    tablet_monitor_->Update(false);
}

void CameraAppHelperImpl::OnScreenStateChanged(ash::ScreenState screen_state) {
  if (screen_state_monitor_.is_bound())
    screen_state_monitor_->Update(ToMojoScreenState(screen_state));
}

}  // namespace chromeos_camera
