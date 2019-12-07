// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_

#include <string>
#include <vector>

#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXRPathHelper {
 public:
  struct DeclaredPaths {
    XrPath microsoft_motion_controller_interaction_profile;
  };

  OpenXRPathHelper();
  ~OpenXRPathHelper();

  XrResult Initialize(XrInstance instance);

  std::vector<std::string> GetInputProfiles(XrPath interaction_profile) const;

  const DeclaredPaths& GetDeclaredPaths() const;

 private:
  bool initialized_{false};

  DeclaredPaths declared_paths_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_