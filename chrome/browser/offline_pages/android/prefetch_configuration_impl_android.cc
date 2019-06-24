// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PrefetchConfiguration_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

using base::android::JavaParamRef;

// These functions fulfill the Java to native link between
// PrefetchConfiguration.java and prefetch_prefs.

namespace offline_pages {
namespace android {

JNI_EXPORT jboolean
JNI_PrefetchConfiguration_IsPrefetchingEnabled(JNIEnv* env) {
  return static_cast<jboolean>(
      prefetch_prefs::IsEnabled(::android::GetMainProfileKey()->GetPrefs()));
}

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsEnabledByServer(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsEnabledByServer(
      ::android::GetMainProfileKey()->GetPrefs()));
}

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsForbiddenCheckDue(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsForbiddenCheckDue(
      ::android::GetMainProfileKey()->GetPrefs()));
}

JNI_EXPORT jboolean
JNI_PrefetchConfiguration_IsEnabledByServerUnknown(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsEnabledByServerUnknown(
      ::android::GetMainProfileKey()->GetPrefs()));
}

JNI_EXPORT void JNI_PrefetchConfiguration_SetPrefetchingEnabledInSettings(
    JNIEnv* env,
    jboolean enabled) {
  prefetch_prefs::SetPrefetchingEnabledInSettings(
      ::android::GetMainProfileKey()->GetPrefs(), enabled);
}

JNI_EXPORT jboolean
JNI_PrefetchConfiguration_IsPrefetchingEnabledInSettings(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsPrefetchingEnabledInSettings(
      ::android::GetMainProfileKey()->GetPrefs()));
}

}  // namespace android
}  // namespace offline_pages
