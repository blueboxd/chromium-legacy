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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

bool GetBooleanForContentSetting(ContentSettingsType type) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  switch (content_settings->GetDefaultContentSetting(type, NULL)) {
    case CONTENT_SETTING_BLOCK:
      return false;
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_ASK:
    default:
      return true;
  }
}

bool IsContentSettingManaged(ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::POLICY_PROVIDER;
}

bool IsContentSettingManagedByCustodian(
    ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::SUPERVISED_PROVIDER;
}

bool IsContentSettingUserModifiable(ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider >= HostContentSettingsMap::PREF_PROVIDER;
}

PrefService* GetPrefService() {
  return GetOriginalProfile()->GetPrefs();
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

static jboolean JNI_PrefServiceBridge_IsContentSettingManaged(
    JNIEnv* env,
    int content_settings_type) {
  return IsContentSettingManaged(
      static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean JNI_PrefServiceBridge_IsContentSettingEnabled(
    JNIEnv* env,
    int content_settings_type) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);
  // Before we migrate functions over to this central function, we must verify
  // that the functionality provided below is correct.
  DCHECK(type == ContentSettingsType::JAVASCRIPT ||
         type == ContentSettingsType::POPUPS ||
         type == ContentSettingsType::ADS ||
         type == ContentSettingsType::CLIPBOARD_READ ||
         type == ContentSettingsType::USB_GUARD ||
         type == ContentSettingsType::BLUETOOTH_SCANNING);
  return GetBooleanForContentSetting(type);
}

static void JNI_PrefServiceBridge_SetContentSettingEnabled(
    JNIEnv* env,
    int content_settings_type,
    jboolean allow) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);

  // Before we migrate functions over to this central function, we must verify
  // that the new category supports ALLOW/BLOCK pairs and, if not, handle them.
  DCHECK(type == ContentSettingsType::JAVASCRIPT ||
         type == ContentSettingsType::POPUPS ||
         type == ContentSettingsType::ADS ||
         type == ContentSettingsType::USB_GUARD ||
         type == ContentSettingsType::BLUETOOTH_SCANNING);

  ContentSetting value = CONTENT_SETTING_BLOCK;
  if (allow) {
    if (type == ContentSettingsType::USB_GUARD ||
        type == ContentSettingsType::BLUETOOTH_SCANNING) {
      value = CONTENT_SETTING_ASK;
    } else {
      value = CONTENT_SETTING_ALLOW;
    }
  }

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(type, value);
}

static void JNI_PrefServiceBridge_SetContentSettingForPattern(
    JNIEnv* env,
    int content_settings_type,
    const JavaParamRef<jstring>& pattern,
    int setting) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(ConvertJavaStringToUTF8(env, pattern)),
      ContentSettingsPattern::Wildcard(),
      static_cast<ContentSettingsType>(content_settings_type), std::string(),
      static_cast<ContentSetting>(setting));
}

static void JNI_PrefServiceBridge_GetContentSettingsExceptions(
    JNIEnv* env,
    int content_settings_type,
    const JavaParamRef<jobject>& list) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  ContentSettingsForOneType entries;
  host_content_settings_map->GetSettingsForOneType(
      static_cast<ContentSettingsType>(content_settings_type), "", &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    Java_PrefServiceBridge_addContentSettingExceptionToList(
        env, list, content_settings_type,
        ConvertUTF8ToJavaString(env, entries[i].primary_pattern.ToString()),
        entries[i].GetContentSetting(),
        ConvertUTF8ToJavaString(env, entries[i].source));
  }
}

static jint JNI_PrefServiceBridge_GetContentSetting(
    JNIEnv* env,
    int content_settings_type) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  return content_settings->GetDefaultContentSetting(
      static_cast<ContentSettingsType>(content_settings_type), nullptr);
}

static void JNI_PrefServiceBridge_SetContentSetting(
    JNIEnv* env,
    int content_settings_type,
    int setting) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      static_cast<ContentSettingsType>(content_settings_type),
      static_cast<ContentSetting>(setting));
}

static jboolean JNI_PrefServiceBridge_GetAcceptCookiesEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::COOKIES);
}

static jboolean JNI_PrefServiceBridge_GetAcceptCookiesUserModifiable(
    JNIEnv* env) {
  return IsContentSettingUserModifiable(ContentSettingsType::COOKIES);
}

static jboolean JNI_PrefServiceBridge_GetAcceptCookiesManagedByCustodian(
    JNIEnv* env) {
  return IsContentSettingManagedByCustodian(ContentSettingsType::COOKIES);
}

static jboolean JNI_PrefServiceBridge_GetAutoplayEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::AUTOPLAY);
}

static jboolean JNI_PrefServiceBridge_GetSensorsEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::SENSORS);
}

static jboolean JNI_PrefServiceBridge_GetSoundEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::SOUND);
}

static jboolean JNI_PrefServiceBridge_GetBackgroundSyncEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::BACKGROUND_SYNC);
}

static jboolean JNI_PrefServiceBridge_GetAutomaticDownloadsEnabled(
    JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::AUTOMATIC_DOWNLOADS);
}

static jboolean JNI_PrefServiceBridge_GetNetworkPredictionEnabled(JNIEnv* env) {
  return GetPrefService()->GetInteger(prefs::kNetworkPredictionOptions)
      != chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

static jboolean JNI_PrefServiceBridge_GetNetworkPredictionManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kNetworkPredictionOptions);
}

static jboolean JNI_PrefServiceBridge_GetNotificationsEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::NOTIFICATIONS);
}

static jboolean JNI_PrefServiceBridge_GetAllowLocationEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::GEOLOCATION);
}

static jboolean JNI_PrefServiceBridge_GetLocationAllowedByPolicy(JNIEnv* env) {
  if (!IsContentSettingManaged(ContentSettingsType::GEOLOCATION))
    return false;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  return content_settings->GetDefaultContentSetting(
             ContentSettingsType::GEOLOCATION, nullptr) ==
         CONTENT_SETTING_ALLOW;
}

static jboolean JNI_PrefServiceBridge_GetAllowLocationUserModifiable(
    JNIEnv* env) {
  return IsContentSettingUserModifiable(ContentSettingsType::GEOLOCATION);
}

static jboolean JNI_PrefServiceBridge_GetAllowLocationManagedByCustodian(
    JNIEnv* env) {
  return IsContentSettingManagedByCustodian(ContentSettingsType::GEOLOCATION);
}

static jboolean JNI_PrefServiceBridge_GetIncognitoModeEnabled(JNIEnv* env) {
  PrefService* prefs = GetPrefService();
  IncognitoModePrefs::Availability incognito_pref =
      IncognitoModePrefs::GetAvailability(prefs);
  DCHECK(incognito_pref == IncognitoModePrefs::ENABLED ||
         incognito_pref == IncognitoModePrefs::DISABLED) <<
             "Unsupported incognito mode preference: " << incognito_pref;
  return incognito_pref != IncognitoModePrefs::DISABLED;
}

static jboolean JNI_PrefServiceBridge_GetIncognitoModeManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kIncognitoModeAvailability);
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

static void JNI_PrefServiceBridge_SetAutoplayEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::AUTOPLAY,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetClipboardEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::CLIPBOARD_READ,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetSensorsEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::SENSORS,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetSoundEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::SOUND,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);

  if (allow) {
    base::RecordAction(
        base::UserMetricsAction("SoundContentSetting.UnmuteBy.DefaultSwitch"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("SoundContentSetting.MuteBy.DefaultSwitch"));
  }
}

static void JNI_PrefServiceBridge_SetAllowCookiesEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetBackgroundSyncEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::BACKGROUND_SYNC,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetAutomaticDownloadsEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::AUTOMATIC_DOWNLOADS,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetAllowLocationEnabled(
    JNIEnv* env,
    jboolean is_enabled) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION,
      is_enabled ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetCameraEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetMicEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::MEDIASTREAM_MIC,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void JNI_PrefServiceBridge_SetNotificationsEnabled(
    JNIEnv* env,
    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::NOTIFICATIONS,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static jboolean JNI_PrefServiceBridge_CanPrefetchAndPrerender(JNIEnv* env) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static jboolean JNI_PrefServiceBridge_GetCameraEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA);
}

static jboolean JNI_PrefServiceBridge_GetCameraUserModifiable(JNIEnv* env) {
  return IsContentSettingUserModifiable(
      ContentSettingsType::MEDIASTREAM_CAMERA);
}

static jboolean JNI_PrefServiceBridge_GetCameraManagedByCustodian(JNIEnv* env) {
  return IsContentSettingManagedByCustodian(
      ContentSettingsType::MEDIASTREAM_CAMERA);
}

static jboolean JNI_PrefServiceBridge_GetMicEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::MEDIASTREAM_MIC);
}

static jboolean JNI_PrefServiceBridge_GetMicUserModifiable(JNIEnv* env) {
  return IsContentSettingUserModifiable(ContentSettingsType::MEDIASTREAM_MIC);
}

static jboolean JNI_PrefServiceBridge_GetMicManagedByCustodian(JNIEnv* env) {
  return IsContentSettingManagedByCustodian(
      ContentSettingsType::MEDIASTREAM_MIC);
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

// static
void PrefServiceBridge::GetAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out) {
  JNIEnv* env = AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env,
      Java_PrefServiceBridge_getAndroidPermissionsForContentSetting(
          env, static_cast<int>(content_settings_type)),
      out);
}

const char* PrefServiceBridge::GetPrefNameExposedToJava(int pref_index) {
  DCHECK_GE(pref_index, 0);
  DCHECK_LT(pref_index, Pref::PREF_NUM_PREFS);
  return kPrefsExposedToJava[pref_index];
}
