// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/permission_request_utils.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "weblayer/browser/java/jni/PermissionRequestUtils_jni.h"

namespace weblayer {

void RequestAndroidPermission(content::WebContents* web_contents,
                              ContentSettingsType content_settings_type,
                              PermissionUpdatedCallback callback) {
  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  auto* window = web_contents->GetTopLevelNativeWindow();
  if (!window) {
    std::move(callback).Run(false);
    return;
  }
  // The callback allocated here will be deleted in the call to OnResult, which
  // is guaranteed to be called.
  Java_PermissionRequestUtils_requestPermission(
      base::android::AttachCurrentThread(), window->GetJavaObject(),
      reinterpret_cast<jlong>(
          new PermissionUpdatedCallback(std::move(callback))),
      static_cast<int>(content_settings_type));
}

void JNI_PermissionRequestUtils_OnResult(JNIEnv* env,
                                         jlong callback_ptr,
                                         jboolean result) {
  auto* callback = reinterpret_cast<PermissionUpdatedCallback*>(callback_ptr);
  std::move(*callback).Run(result);
  delete callback;
}

}  // namespace weblayer
