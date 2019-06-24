// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/NotificationSchedulerTask_jni.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"

// static
void JNI_NotificationSchedulerTask_OnStartTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jobject>& j_callback) {
  // TODO(Hesen): Pass task finish callback to the scheduler.
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* service =
      NotificationScheduleServiceFactory::GetForBrowserContext(profile);
  auto* handler = service->GetBackgroundTaskSchedulerHandler();
  handler->OnStartTask();
}

// static
jboolean JNI_NotificationSchedulerTask_OnStopTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* service =
      NotificationScheduleServiceFactory::GetForBrowserContext(profile);
  auto* handler = service->GetBackgroundTaskSchedulerHandler();
  handler->OnStopTask();
  // TODO(Hesen): Handle the return value and stoptask.
  return true;
}

NotificationBackgroundTaskSchedulerAndroid::
    NotificationBackgroundTaskSchedulerAndroid() = default;

NotificationBackgroundTaskSchedulerAndroid::
    ~NotificationBackgroundTaskSchedulerAndroid() = default;

void NotificationBackgroundTaskSchedulerAndroid::Schedule(
    notifications::SchedulerTaskTime scheduler_task_time,
    base::TimeDelta window_start,
    base::TimeDelta window_end) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NotificationSchedulerTask_schedule(
      env, static_cast<jint>(scheduler_task_time),
      base::saturated_cast<jlong>(window_start.InMilliseconds()),
      base::saturated_cast<jlong>(window_end.InMilliseconds()));
}
