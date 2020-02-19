// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_service_bridge_android.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/UpdateNotificationServiceBridge_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/updates/update_notification_info.h"
#include "chrome/browser/updates/update_notification_service.h"
#include "chrome/browser/updates/update_notification_service_factory.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace updates {

//
// Java -> C++
//
void JNI_UpdateNotificationServiceBridge_Schedule(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jstring>& j_message,
    const jint j_state,
    const jboolean j_show_immediately) {
  ProfileKey* profile_key = ::android::GetLastUsedProfileKey();
  auto* update_notification_service =
      UpdateNotificationServiceFactory::GetForKey(profile_key);
  UpdateNotificationInfo data;
  data.title = ConvertJavaStringToUTF16(env, j_title);
  data.message = ConvertJavaStringToUTF16(env, j_message);
  data.state = static_cast<int>(j_state);
  data.should_show_immediately = static_cast<bool>(j_show_immediately);
  update_notification_service->Schedule(std::move(data));
}

//
// C++ -> Java
//
void UpdateNotificationServiceBridgeAndroid::UpdateLastShownTimeStamp(
    base::Time timestamp) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UpdateNotificationServiceBridge_updateLastShownTimeStamp(
      env, timestamp.ToJavaTime());
}

base::Optional<base::Time>
UpdateNotificationServiceBridgeAndroid::GetLastShownTimeStamp() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto timestamp =
      Java_UpdateNotificationServiceBridge_getLastShownTimeStamp(env);
  return timestamp == 0
             ? base::nullopt
             : (base::make_optional(base::Time::FromJavaTime(timestamp)));
}

void UpdateNotificationServiceBridgeAndroid::UpdateThrottleInterval(
    base::TimeDelta interval) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UpdateNotificationServiceBridge_updateThrottleInterval(
      env, interval.InMilliseconds());
}

base::Optional<base::TimeDelta>
UpdateNotificationServiceBridgeAndroid::GetThrottleInterval() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto interval = Java_UpdateNotificationServiceBridge_getThrottleInterval(env);
  return interval == 0 ? base::nullopt
                       : (base::make_optional(
                             base::TimeDelta::FromMilliseconds(interval)));
}

void UpdateNotificationServiceBridgeAndroid::UpdateNegativeActionCount(
    int count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UpdateNotificationServiceBridge_updateNegativeActionCount(env, count);
}

int UpdateNotificationServiceBridgeAndroid::GetNegativeActionCount() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_UpdateNotificationServiceBridge_getNegativeActionCount(env);
}

void UpdateNotificationServiceBridgeAndroid::LaunchChromeActivity(int state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UpdateNotificationServiceBridge_launchChromeActivity(
      env, static_cast<jint>(state));
}
}  // namespace updates
