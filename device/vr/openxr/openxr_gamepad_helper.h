// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_GAMEPAD_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_GAMEPAD_HELPER_H_

#include "device/vr/openxr/openxr_controller.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"

namespace device {

class OpenXrGamepadHelper {
 public:
  static XrResult CreateOpenXrGamepadHelper(
      XrInstance instance,
      XrSession session,
      XrSpace local_space,
      std::unique_ptr<OpenXrGamepadHelper>* helper);

  OpenXrGamepadHelper(XrSession session, XrSpace local_space);

  ~OpenXrGamepadHelper();

  mojom::XRGamepadDataPtr GetGamepadData(XrTime predicted_display_time);

 private:
  XrSession session_;
  XrSpace local_space_;

  std::array<OpenXrController,
             static_cast<size_t>(OpenXrControllerType::kCount)>
      controllers_;
  std::array<XrActiveActionSet,
             static_cast<size_t>(OpenXrControllerType::kCount)>
      active_action_sets_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrGamepadHelper);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GAMEPAD_HELPER_H_
