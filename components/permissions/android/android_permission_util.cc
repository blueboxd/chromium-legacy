// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/android_permission_util.h"

#include "base/android/jni_array.h"
#include "components/permissions/android/jni_headers/PermissionUtil_jni.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace permissions {

void GetAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env,
      Java_PermissionUtil_getAndroidPermissionsForContentSetting(
          env, static_cast<int>(content_settings_type)),
      out);
}

PermissionRepromptState ShouldRepromptUserForPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types) {
  if (!web_contents)
    return PermissionRepromptState::kCannotShow;

  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android)
    return PermissionRepromptState::kCannotShow;

  for (ContentSettingsType content_settings_type : content_settings_types) {
    std::vector<std::string> android_permissions;
    GetAndroidPermissionsForContentSetting(content_settings_type,
                                           &android_permissions);

    for (const auto& android_permission : android_permissions) {
      if (!window_android->HasPermission(android_permission)) {
        PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
            true, content_settings_types);
        return PermissionRepromptState::kShow;
      }
    }
  }

  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
      false, content_settings_types);
  return PermissionRepromptState::kNoNeed;
}

}  // namespace permissions
