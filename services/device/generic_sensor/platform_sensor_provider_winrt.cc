// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"

#include <comdef.h>

#include "base/memory/singleton.h"

namespace device {

PlatformSensorProviderWinrt::PlatformSensorProviderWinrt() = default;

PlatformSensorProviderWinrt::~PlatformSensorProviderWinrt() = default;

void PlatformSensorProviderWinrt::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback) {
  callback.Run(nullptr);
}

}  // namespace device