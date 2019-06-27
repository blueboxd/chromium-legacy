// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"

#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/time/default_tick_clock.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

TabletModeControllerTestApi::TabletModeControllerTestApi()
    : tablet_mode_controller_(Shell::Get()->tablet_mode_controller()) {}

TabletModeControllerTestApi::~TabletModeControllerTestApi() = default;

void TabletModeControllerTestApi::EnterTabletMode() {
  tablet_mode_controller_->SetEnabledForTest(true);
}

void TabletModeControllerTestApi::LeaveTabletMode() {
  tablet_mode_controller_->SetEnabledForTest(false);
}

void TabletModeControllerTestApi::AttachExternalMouse() {
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  tablet_mode_controller_->OnInputDeviceConfigurationChanged(
      ui::InputDeviceEventObserver::kMouse);
}

void TabletModeControllerTestApi::TriggerLidUpdate(const gfx::Vector3dF& lid) {
  scoped_refptr<AccelerometerUpdate> update(new AccelerometerUpdate());
  update->Set(ACCELEROMETER_SOURCE_SCREEN, false, lid.x(), lid.y(), lid.z());
  tablet_mode_controller_->OnAccelerometerUpdated(update);
}

void TabletModeControllerTestApi::TriggerBaseAndLidUpdate(
    const gfx::Vector3dF& base,
    const gfx::Vector3dF& lid) {
  scoped_refptr<AccelerometerUpdate> update(new AccelerometerUpdate());
  update->Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, false,
              base.x(), base.y(), base.z());
  update->Set(ACCELEROMETER_SOURCE_SCREEN, false, lid.x(), lid.y(), lid.z());
  tablet_mode_controller_->OnAccelerometerUpdated(update);
}

void TabletModeControllerTestApi::OpenLidToAngle(float degrees) {
  DCHECK(degrees >= 0.0f);
  DCHECK(degrees <= 360.0f);

  float radians = degrees * kDegreesToRadians;
  gfx::Vector3dF base_vector(0.0f, -kMeanGravity, 0.0f);
  gfx::Vector3dF lid_vector(0.0f, kMeanGravity * cos(radians),
                            kMeanGravity * sin(radians));
  TriggerBaseAndLidUpdate(base_vector, lid_vector);
}

void TabletModeControllerTestApi::HoldDeviceVertical() {
  gfx::Vector3dF base_vector(9.8f, 0.0f, 0.0f);
  gfx::Vector3dF lid_vector(9.8f, 0.0f, 0.0f);
  TriggerBaseAndLidUpdate(base_vector, lid_vector);
}

void TabletModeControllerTestApi::OpenLid() {
  tablet_mode_controller_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::OPEN, tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::CloseLid() {
  tablet_mode_controller_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::CLOSED, tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::SetTabletMode(bool on) {
  tablet_mode_controller_->TabletModeEventReceived(
      on ? chromeos::PowerManagerClient::TabletMode::ON
         : chromeos::PowerManagerClient::TabletMode::OFF,
      tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::SuspendImminent() {
  tablet_mode_controller_->SuspendImminent(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);
}

void TabletModeControllerTestApi::SuspendDone(base::TimeDelta sleep_duration) {
  tablet_mode_controller_->SuspendDone(sleep_duration);
}

}  // namespace ash
