// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"

#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtFactory::Create(mojom::SensorType) {
  return nullptr;
}

}  // namespace device