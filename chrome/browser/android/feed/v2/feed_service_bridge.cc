// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_service_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "chrome/android/chrome_jni_headers/FeedServiceBridge_jni.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_service.h"

namespace feed {

static jboolean JNI_FeedServiceBridge_IsEnabled(JNIEnv* env) {
  return FeedServiceBridge::IsEnabled();
}

std::string FeedServiceBridge::GetLanguageTag() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return ConvertJavaStringToUTF8(env,
                                 Java_FeedServiceBridge_getLanguageTag(env));
}

DisplayMetrics FeedServiceBridge::GetDisplayMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<double> numbers;
  base::android::JavaDoubleArrayToDoubleVector(
      env, Java_FeedServiceBridge_getDisplayMetrics(env), &numbers);
  DCHECK_EQ(3UL, numbers.size());
  DisplayMetrics result;
  result.density = numbers[0];
  result.width_pixels = numbers[1];
  result.height_pixels = numbers[2];
  return result;
}

bool FeedServiceBridge::IsEnabled() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  return FeedServiceFactory::GetForBrowserContext(profile)->IsEnabled(
      *profile->GetPrefs());
}

}  // namespace feed
