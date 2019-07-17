// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_

#include <memory>

#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"

namespace device {

namespace mojom {
enum class SensorType;
}

// Helper class used to create PlatformSensorReaderWinBasert instances
class PlatformSensorReaderWinrtFactory {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create(
      mojom::SensorType type);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_