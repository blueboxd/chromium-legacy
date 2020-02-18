// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hfp_battery_listener.h"

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {

HfpBatteryListener::HfpBatteryListener(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {
  DCHECK(adapter);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
}

HfpBatteryListener::~HfpBatteryListener() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void HfpBatteryListener::OnBluetoothBatteryChanged(const std::string& address,
                                                   uint32_t level) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device)
    return;

  if (level > 100)
    device->SetBatteryPercentage(base::nullopt);
  else
    device->SetBatteryPercentage(level);
}

}  // namespace ash
