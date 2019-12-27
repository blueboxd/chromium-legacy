// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util.h"

#include "device/vr/openxr/openxr_path_helper.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXRPathHelper::OpenXRPathHelper() {}

OpenXRPathHelper::~OpenXRPathHelper() = default;

XrResult OpenXRPathHelper::Initialize(XrInstance instance) {
  DCHECK(!initialized_);

  // Create path declarations
  for (const auto& profile : kOpenXrControllerInteractionProfiles) {
    RETURN_IF_XR_FAILED(
        xrStringToPath(instance, profile.path,
                       &(declared_interaction_profile_paths_[profile.type])));
  }
  initialized_ = true;

  return XR_SUCCESS;
}

OpenXrInteractionProfileType OpenXRPathHelper::GetInputProfileType(
    XrPath interaction_profile) const {
  DCHECK(initialized_);
  for (auto it : declared_interaction_profile_paths_) {
    if (it.second == interaction_profile) {
      return it.first;
    }
  }
  return OpenXrInteractionProfileType::kCount;
}

std::vector<std::string> OpenXRPathHelper::GetInputProfiles(
    OpenXrInteractionProfileType interaction_profile) const {
  DCHECK(initialized_);

  for (auto& it : kOpenXrControllerInteractionProfiles) {
    if (it.type == interaction_profile) {
      const char* const* const input_profiles = it.input_profiles;
      return std::vector<std::string>(input_profiles,
                                      input_profiles + it.profile_size);
    }
  }

  return {};
}

XrPath OpenXRPathHelper::GetInteractionProfileXrPath(
    OpenXrInteractionProfileType type) const {
  if (type == OpenXrInteractionProfileType::kCount) {
    return XR_NULL_PATH;
  }
  return declared_interaction_profile_paths_.at(type);
}

}  // namespace device
