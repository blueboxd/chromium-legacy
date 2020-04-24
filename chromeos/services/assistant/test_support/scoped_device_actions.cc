// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/scoped_device_actions.h"

#include <utility>

namespace chromeos {
namespace assistant {

void ScopedDeviceActions::GetScreenBrightnessLevel(
    GetScreenBrightnessLevelCallback callback) {
  std::move(callback).Run(/*success=*/true, current_brightness_);
}

bool ScopedDeviceActions::OpenAndroidApp(
    chromeos::assistant::mojom::AndroidAppInfoPtr app_info) {
  return true;
}

}  // namespace assistant
}  // namespace chromeos
