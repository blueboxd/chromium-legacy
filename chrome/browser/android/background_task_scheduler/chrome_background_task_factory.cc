// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"

#include "chrome/android/chrome_jni_headers/ChromeBackgroundTaskFactory_jni.h"

// static
void ChromeBackgroundTaskFactory::SetAsDefault() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeBackgroundTaskFactory_setAsDefault(env);
}

std::unique_ptr<background_task::BackgroundTask>
ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId(int task_id) {
  // Add your tasks here with mappings to the given task_id.
  return nullptr;
}
