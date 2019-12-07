// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "device/vr/openxr/openxr_path_helper.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXRPathHelper::OpenXRPathHelper() {}

OpenXRPathHelper::~OpenXRPathHelper() = default;

XrResult OpenXRPathHelper::Initialize(XrInstance instance) {
  DCHECK(!initialized_);

  // Create path declarations
  RETURN_IF_XR_FAILED(xrStringToPath(
      instance, "/interaction_profiles/microsoft/motion_controller",
      &declared_paths_.microsoft_motion_controller_interaction_profile));

  initialized_ = true;

  return XR_SUCCESS;
}

std::vector<std::string> OpenXRPathHelper::GetInputProfiles(
    XrPath interaction_profile) const {
  DCHECK(initialized_);

  if (interaction_profile ==
      declared_paths_.microsoft_motion_controller_interaction_profile) {
    return {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"};
  }

  return {};
}

const OpenXRPathHelper::DeclaredPaths& OpenXRPathHelper::GetDeclaredPaths()
    const {
  DCHECK(initialized_);
  return declared_paths_;
}

}  // namespace device
