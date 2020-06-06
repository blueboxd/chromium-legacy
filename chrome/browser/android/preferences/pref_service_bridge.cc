// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/preferences/jni_headers/PrefServiceBridge_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace {

using base::android::JavaParamRef;

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static void JNI_PrefServiceBridge_ClearPref(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  GetPrefService()->ClearPref(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

static jboolean JNI_PrefServiceBridge_GetBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return GetPrefService()->GetBoolean(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

static void JNI_PrefServiceBridge_SetBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference,
    const jboolean j_value) {
  GetPrefService()->SetBoolean(
      base::android::ConvertJavaStringToUTF8(env, j_preference), j_value);
}

static jint JNI_PrefServiceBridge_GetInteger(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return GetPrefService()->GetInteger(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

static void JNI_PrefServiceBridge_SetInteger(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference,
    const jint j_value) {
  GetPrefService()->SetInteger(
      base::android::ConvertJavaStringToUTF8(env, j_preference), j_value);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_PrefServiceBridge_GetString(JNIEnv* env,
                                const JavaParamRef<jstring>& j_preference) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(
               base::android::ConvertJavaStringToUTF8(env, j_preference)));
}

static void JNI_PrefServiceBridge_SetString(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference,
    const base::android::JavaParamRef<jstring>& j_value) {
  GetPrefService()->SetString(
      base::android::ConvertJavaStringToUTF8(env, j_preference),
      base::android::ConvertJavaStringToUTF8(env, j_value));
}

static jboolean JNI_PrefServiceBridge_IsManagedPreference(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return GetPrefService()->IsManagedPreference(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}
