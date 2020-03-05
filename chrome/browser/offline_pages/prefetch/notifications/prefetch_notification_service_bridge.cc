// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_bridge.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/PrefetchNotificationServiceBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace prefetch {

void JNI_PrefetchNotificationServiceBridge_Schedule(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jstring>& j_message) {
  NOTIMPLEMENTED();
}

void UpdateIsUnderSuppression(bool is_under_suppression) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrefetchNotificationServiceBridge_updateIsUnderSuppression(
      env, is_under_suppression);
}

void LaunchDownloadHome() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrefetchNotificationServiceBridge_launchDownloadHome(env);
}

}  // namespace prefetch
}  // namespace offline_pages
