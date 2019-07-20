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
using base::android::ScopedJavaLocalRef;

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
    jint j_client_type,
    const base::android::JavaParamRef<jstring>& j_guid) {
  GetUserActionHandler(j_profile)->OnClick(
      static_cast<notifications::SchedulerClientType>(j_client_type),
      ConvertJavaStringToUTF8(env, j_guid));
}

// static
void JNI_DisplayAgent_OnDismiss(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint j_client_type,
    const base::android::JavaParamRef<jstring>& j_guid) {
  GetUserActionHandler(j_profile)->OnDismiss(
      static_cast<notifications::SchedulerClientType>(j_client_type),
      ConvertJavaStringToUTF8(env, j_guid));
}

// static
void JNI_DisplayAgent_OnActionButton(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint j_client_type,
    const base::android::JavaParamRef<jstring>& j_guid,
    jint type) {
  GetUserActionHandler(j_profile)->OnActionClick(
      static_cast<notifications::SchedulerClientType>(j_client_type),
      ConvertJavaStringToUTF8(env, j_guid),
      static_cast<notifications::ActionButtonType>(type));
}

DisplayAgentAndroid::DisplayAgentAndroid() = default;

DisplayAgentAndroid::~DisplayAgentAndroid() = default;

void DisplayAgentAndroid::ShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    std::unique_ptr<SystemData> system_data) {
  // TODO(xingliu): Refactor and hook to NotificationDisplayService.
  DCHECK(notification_data);
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!notification_data->title.empty());
  DCHECK(!notification_data->message.empty());

  // Wrap button info. Retrieving class name in run time must be guarded with
  // test.
  auto java_notification_data = Java_DisplayAgent_buildNotificationData(
      env, ConvertUTF16ToJavaString(env, notification_data->title),
      ConvertUTF16ToJavaString(env, notification_data->message),
      nullptr /*icon*/);

  for (size_t i = 0; i < notification_data->buttons.size(); ++i) {
    const auto& button = notification_data->buttons[i];
    Java_DisplayAgent_addButton(
        env, java_notification_data, ConvertUTF16ToJavaString(env, button.text),
        static_cast<int>(button.type), ConvertUTF8ToJavaString(env, button.id));
  }

  auto java_system_data = Java_DisplayAgent_buildSystemData(
      env, static_cast<int>(system_data->type),
      ConvertUTF8ToJavaString(env, system_data->guid));

  ShowNotificationInternal(env, java_notification_data, java_system_data);
}

void DisplayAgentAndroid::ShowNotificationInternal(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_notification_data,
    const base::android::JavaRef<jobject>& java_system_data) {
  Java_DisplayAgent_showNotification(env, java_notification_data,
                                     java_system_data);
}
