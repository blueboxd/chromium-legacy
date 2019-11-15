// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/pref_service_bridge.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/PrefServiceBridge_jni.h"
#include "chrome/browser/android/preferences/prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_thread.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jboolean JNI_PrefServiceBridge_GetBoolean(
    JNIEnv* env,
    const jint j_pref_index) {
  return GetPrefService()->GetBoolean(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index));
}

static void JNI_PrefServiceBridge_SetBoolean(JNIEnv* env,
                                             const jint j_pref_index,
                                             const jboolean j_value) {
  GetPrefService()->SetBoolean(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index), j_value);
}

static jint JNI_PrefServiceBridge_GetInteger(JNIEnv* env,
                                             const jint j_pref_index) {
  return GetPrefService()->GetInteger(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index));
}

static void JNI_PrefServiceBridge_SetInteger(JNIEnv* env,
                                             const jint j_pref_index,
                                             const jint j_value) {
  GetPrefService()->SetInteger(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index), j_value);
}

static ScopedJavaLocalRef<jstring> JNI_PrefServiceBridge_GetString(
    JNIEnv* env,
    const jint j_pref_index) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(
               PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index)));
}

static void JNI_PrefServiceBridge_SetString(
    JNIEnv* env,
    const jint j_pref_index,
    const JavaParamRef<jstring>& j_value) {
  GetPrefService()->SetString(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index),
      ConvertJavaStringToUTF8(env, j_value));
}

static jboolean JNI_PrefServiceBridge_IsManagedPreference(
    JNIEnv* env,
    const jint j_pref_index) {
  return GetPrefService()->IsManagedPreference(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index));
}

static jboolean JNI_PrefServiceBridge_GetNetworkPredictionEnabled(JNIEnv* env) {
  return GetPrefService()->GetInteger(prefs::kNetworkPredictionOptions)
      != chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

static jboolean JNI_PrefServiceBridge_GetNetworkPredictionManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kNetworkPredictionOptions);
}

static jboolean JNI_PrefServiceBridge_IsMetricsReportingEnabled(JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

static void JNI_PrefServiceBridge_SetMetricsReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, enabled);
}

static jboolean JNI_PrefServiceBridge_IsMetricsReportingManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      metrics::prefs::kMetricsReportingEnabled);
}

static jboolean JNI_PrefServiceBridge_CanPrefetchAndPrerender(JNIEnv* env) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static void JNI_PrefServiceBridge_SetNetworkPredictionEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetInteger(
      prefs::kNetworkPredictionOptions,
      enabled ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
              : chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

static jboolean
JNI_PrefServiceBridge_ObsoleteNetworkPredictionOptionsHasUserSetting(
    JNIEnv* env) {
  return GetPrefService()->GetUserPrefValue(
      prefs::kNetworkPredictionOptions) != NULL;
}

static jboolean JNI_PrefServiceBridge_GetFirstRunEulaAccepted(JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

static void JNI_PrefServiceBridge_SetEulaAccepted(JNIEnv* env) {
  g_browser_process->local_state()->SetBoolean(prefs::kEulaAccepted, true);
}

const char* PrefServiceBridge::GetPrefNameExposedToJava(int pref_index) {
  DCHECK_GE(pref_index, 0);
  DCHECK_LT(pref_index, Pref::PREF_NUM_PREFS);
  return kPrefsExposedToJava[pref_index];
}
