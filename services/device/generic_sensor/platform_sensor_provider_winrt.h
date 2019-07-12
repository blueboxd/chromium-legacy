// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace device {

class PlatformSensorReaderWin;

// Implementation of PlatformSensorProvider for Windows platform using the
// Windows.Devices.Sensors WinRT API. PlatformSensorProviderWinrt is
// responsible for the following tasks:
// - Starts sensor thread and stops it when there are no active sensors.
// - Creates sensor reader.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class PlatformSensorProviderWinrt final : public PlatformSensorProvider {
 public:
  PlatformSensorProviderWinrt();
  ~PlatformSensorProviderWinrt() override;

 protected:
  // PlatformSensorProvider interface implementation.
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            const CreateSensorCallback& callback) override;

 private:
  friend struct base::DefaultSingletonTraits<PlatformSensorProviderWinrt>;

  void SensorReaderCreated(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      const CreateSensorCallback& callback,
      std::unique_ptr<PlatformSensorReaderWin> sensor_reader);

  PlatformSensorProviderWinrt(const PlatformSensorProviderWinrt&) = delete;
  PlatformSensorProviderWinrt& operator=(const PlatformSensorProviderWinrt&) =
      delete;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WINRT_H_