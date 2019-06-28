// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/display_agent_android.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/DisplayAgent_jni.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

notifications::UserActionHandler* GetUserActionHandler(
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* service =
      NotificationScheduleServiceFactory::GetForBrowserContext(profile);
  return service->GetUserActionHandler();
}

}  // namespace

// static
void JNI_DisplayAgent_OnContentClick(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& j_guid) {
  GetUserActionHandler(j_profile)->OnClick(
      ConvertJavaStringToUTF8(env, j_guid));
}

// static
void JNI_DisplayAgent_OnDismiss(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& j_guid) {
  GetUserActionHandler(j_profile)->OnDismiss(
      ConvertJavaStringToUTF8(env, j_guid));
}

// static
void JNI_DisplayAgent_OnActionButton(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& guid) {
  // TODO(xingliu): Support buttons.
  NOTIMPLEMENTED();
}

DisplayAgentAndroid::DisplayAgentAndroid() = default;

DisplayAgentAndroid::~DisplayAgentAndroid() = default;

void DisplayAgentAndroid::ShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data) {
  // TODO(xingliu): Refactor and hook to NotificationDisplayService.
  DCHECK(notification_data);
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!notification_data->title.empty());
  DCHECK(!notification_data->message.empty());

  // TODO(xingliu): Populate the icon to Java.
  auto java_notification_data = Java_DisplayAgent_Constructor(
      env, ConvertUTF8ToJavaString(env, notification_data->id),
      ConvertUTF16ToJavaString(env, notification_data->title),
      ConvertUTF16ToJavaString(env, notification_data->message),
      nullptr /*icon*/);

  Java_DisplayAgent_showNotification(env, java_notification_data);
}
