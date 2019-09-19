// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_
#define COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_

#include <vector>

#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"

namespace chromeos_camera {

class CameraAppHelperImpl : public cros::mojom::CameraAppHelper {
 public:
  using IntentCallback = base::RepeatingCallback<
      void(uint32_t, bool, const std::vector<uint8_t>&)>;

  explicit CameraAppHelperImpl(IntentCallback intent_callback);
  ~CameraAppHelperImpl() override;

  // cros::mojom::CameraAppHelper implementations.
  void OnIntentHandled(uint32_t intent_id,
                       bool is_success,
                       const std::vector<uint8_t>& captured_data) override;
  void IsTabletMode(IsTabletModeCallback callback) override;

 private:
  IntentCallback intent_callback_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppHelperImpl);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_