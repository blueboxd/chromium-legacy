// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_
#define COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_

#include <string>
#include <vector>

#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace permissions {

// Populate the list of corresponding Android permissions associated with the
// ContentSettingsType specified.
void GetAndroidPermissionsForContentSetting(ContentSettingsType content_type,
                                            std::vector<std::string>* out);

// The states that indicate if the user should/can be re-nudged to accept
// permissions. In Chrome this correlates to the PermissionUpdateInfoBar.
enum class PermissionRepromptState {
  // No need to show the infobar as the permissions have been already granted.
  kNoNeed = 0,
  // Show the the permission infobar.
  kShow,
  // Can't show the permission infobar due to an internal state issue like
  // the WebContents or the AndroidWindow are not available.
  kCannotShow,
};

PermissionRepromptState ShouldRepromptUserForPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_
