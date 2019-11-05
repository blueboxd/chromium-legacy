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

#include "base/android/build_info.h"
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
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/version_info/version_info.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

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

static jboolean JNI_PrefServiceBridge_GetPasswordEchoEnabled(JNIEnv* env) {
  return GetPrefService()->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
}

static jboolean JNI_PrefServiceBridge_GetTranslateEnabled(JNIEnv* env) {
  return GetPrefService()->GetBoolean(prefs::kOfferTranslateEnabled);
}

static jboolean JNI_PrefServiceBridge_GetTranslateManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(prefs::kOfferTranslateEnabled);
}

static jboolean JNI_PrefServiceBridge_GetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env) {
  return safe_browsing::IsExtendedReportingEnabled(*GetPrefService());
}

static void JNI_PrefServiceBridge_SetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  safe_browsing::SetExtendedReportingPrefAndMetric(
      GetPrefService(), enabled,
      safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS);
}

static jboolean JNI_PrefServiceBridge_GetSafeBrowsingExtendedReportingManaged(
    JNIEnv* env) {
  PrefService* pref_service = GetPrefService();
  return pref_service->IsManagedPreference(
      prefs::kSafeBrowsingScoutReportingEnabled);
}

static jboolean JNI_PrefServiceBridge_GetSafeBrowsingEnabled(JNIEnv* env) {
  return GetPrefService()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

static void JNI_PrefServiceBridge_SetSafeBrowsingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kSafeBrowsingEnabled, enabled);
}

static jboolean JNI_PrefServiceBridge_GetSafeBrowsingManaged(JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(prefs::kSafeBrowsingEnabled);
}

static jboolean JNI_PrefServiceBridge_GetNotificationsEnabled(JNIEnv* env) {
  return GetBooleanForContentSetting(ContentSettingsType::NOTIFICATIONS);
}

static jboolean JNI_PrefServiceBridge_GetNotificationsVibrateEnabled(
    JNIEnv* env) {
  return GetPrefService()->GetBoolean(prefs::kNotificationsVibrateEnabled);
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

static jboolean JNI_PrefServiceBridge_GetResolveNavigationErrorEnabled(
    JNIEnv* env) {
  return GetPrefService()->GetBoolean(prefs::kAlternateErrorPagesEnabled);
}

static jboolean JNI_PrefServiceBridge_GetResolveNavigationErrorManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kAlternateErrorPagesEnabled);
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

static void JNI_PrefServiceBridge_SetNotificationsVibrateEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kNotificationsVibrateEnabled, enabled);
}

static jboolean JNI_PrefServiceBridge_CanPrefetchAndPrerender(JNIEnv* env) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static ScopedJavaLocalRef<jstring> JNI_PrefServiceBridge_GetSyncLastAccountName(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kGoogleServicesLastUsername));
}

static void JNI_PrefServiceBridge_SetTranslateEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kOfferTranslateEnabled, enabled);
}

static void JNI_PrefServiceBridge_SetPasswordEchoEnabled(
    JNIEnv* env,
    jboolean passwordEchoEnabled) {
  GetPrefService()->SetBoolean(prefs::kWebKitPasswordEchoEnabled,
                               passwordEchoEnabled);
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

static void JNI_PrefServiceBridge_SetResolveNavigationErrorEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
}

static jboolean JNI_PrefServiceBridge_GetFirstRunEulaAccepted(JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

static void JNI_PrefServiceBridge_SetEulaAccepted(JNIEnv* env) {
  g_browser_process->local_state()->SetBoolean(prefs::kEulaAccepted, true);
}

static void JNI_PrefServiceBridge_ResetAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jstring>& default_locale) {
  std::string accept_languages(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
  std::string locale_string(ConvertJavaStringToUTF8(env, default_locale));

  PrefServiceBridge::PrependToAcceptLanguagesIfNecessary(locale_string,
                                                         &accept_languages);
  GetPrefService()->SetString(language::prefs::kAcceptLanguages,
                              accept_languages);
}

// Sends all information about the different versions to Java.
// From browser_about_handler.cc
static ScopedJavaLocalRef<jobject> JNI_PrefServiceBridge_GetAboutVersionStrings(
    JNIEnv* env) {
  std::string os_version = version_info::GetOSType();
  os_version += " " + AndroidAboutAppInfo::GetOsInfo();

  base::android::BuildInfo* android_build_info =
        base::android::BuildInfo::GetInstance();
  std::string application(android_build_info->host_package_label());
  application.append(" ");
  application.append(version_info::GetVersionNumber());

  return Java_PrefServiceBridge_createAboutVersionStrings(
      env, ConvertUTF8ToJavaString(env, application),
      ConvertUTF8ToJavaString(env, os_version));
}

// static
// This logic should be kept in sync with prependToAcceptLanguagesIfNecessary in
// chrome/android/java/src/org/chromium/chrome/browser/
//     physicalweb/PwsClientImpl.java
// Input |locales| is a comma separated locale representation that consists of
// language tags (BCP47 compliant format). Each language tag contains a language
// code and a country code or a language code only.
void PrefServiceBridge::PrependToAcceptLanguagesIfNecessary(
    const std::string& locales,
    std::string* accept_languages) {
  std::vector<std::string> locale_list =
      base::SplitString(locales + "," + *accept_languages, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::set<std::string> seen_tags;
  std::vector<std::pair<std::string, std::string>> unique_locale_list;
  for (const std::string& locale_str : locale_list) {
    char locale_ID[ULOC_FULLNAME_CAPACITY] = {};
    char language_code_buffer[ULOC_LANG_CAPACITY] = {};
    char country_code_buffer[ULOC_COUNTRY_CAPACITY] = {};

    UErrorCode error = U_ZERO_ERROR;
    uloc_forLanguageTag(locale_str.c_str(), locale_ID, ULOC_FULLNAME_CAPACITY,
                        nullptr, &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getLanguage(locale_ID, language_code_buffer, ULOC_LANG_CAPACITY,
                     &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getCountry(locale_ID, country_code_buffer, ULOC_COUNTRY_CAPACITY,
                    &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    std::string language_code(language_code_buffer);
    std::string country_code(country_code_buffer);
    std::string language_tag(language_code + "-" + country_code);

    if (seen_tags.find(language_tag) != seen_tags.end())
      continue;

    seen_tags.insert(language_tag);
    unique_locale_list.push_back(std::make_pair(language_code, country_code));
  }

  // If language is not in the accept languages list, also add language
  // code. A language code should only be inserted after the last
  // languageTag that contains that language.
  // This will work with the IDS_ACCEPT_LANGUAGE localized strings bundled
  // with Chrome but may fail on arbitrary lists of language tags due to
  // differences in case and whitespace.
  std::set<std::string> seen_languages;
  std::vector<std::string> output_list;
  for (auto it = unique_locale_list.rbegin(); it != unique_locale_list.rend();
       ++it) {
    if (seen_languages.find(it->first) == seen_languages.end()) {
      output_list.push_back(it->first);
      seen_languages.insert(it->first);
    }
    if (!it->second.empty())
      output_list.push_back(it->first + "-" + it->second);
  }

  std::reverse(output_list.begin(), output_list.end());
  *accept_languages = base::JoinString(output_list, ",");
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

static void JNI_PrefServiceBridge_GetChromeAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& list) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<translate::TranslateLanguageInfo> languages;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  translate_prefs->GetLanguageInfoList(
      app_locale, translate_prefs->IsTranslateAllowedByPolicy(), &languages);

  language::ToTranslateLanguageSynonym(&app_locale);
  for (const auto& info : languages) {
    // If the language comes from the same language family as the app locale,
    // translate for this language won't be supported on this device.
    std::string lang_code = info.code;
    language::ToTranslateLanguageSynonym(&lang_code);
    bool supports_translate =
        info.supports_translate && lang_code != app_locale;

    Java_PrefServiceBridge_addNewLanguageItemToList(
        env, list, ConvertUTF8ToJavaString(env, info.code),
        ConvertUTF8ToJavaString(env, info.display_name),
        ConvertUTF8ToJavaString(env, info.native_display_name),
        supports_translate);
  }
}

static void JNI_PrefServiceBridge_GetUserAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& list) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  Java_PrefServiceBridge_copyStringArrayToList(
      env, list, ToJavaArrayOfStrings(env, languages));
}

static void JNI_PrefServiceBridge_SetLanguageOrder(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_order) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::vector<std::string> order;
  const int num_langs = (*env).GetArrayLength(j_order);
  for (int i = 0; i < num_langs; i++) {
    jstring string = (jstring)(*env).GetObjectArrayElement(j_order, i);
    order.push_back((*env).GetStringUTFChars(string, nullptr));
  }
  translate_prefs->SetLanguageOrder(order);
}

static void JNI_PrefServiceBridge_UpdateUserAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jboolean is_add) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (is_add) {
    translate_prefs->AddToLanguageList(language_code, false /*force_blocked=*/);
  } else {
    translate_prefs->RemoveFromLanguageList(language_code);
  }
}

static void JNI_PrefServiceBridge_MoveAcceptLanguage(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jint offset) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);

  std::string language_code(ConvertJavaStringToUTF8(env, language));

  translate::TranslatePrefs::RearrangeSpecifier where =
      translate::TranslatePrefs::kNone;

  if (offset > 0) {
    where = translate::TranslatePrefs::kDown;
  } else {
    offset = -offset;
    where = translate::TranslatePrefs::kUp;
  }

  translate_prefs->RearrangeLanguage(language_code, where, offset, languages);
}

static jboolean JNI_PrefServiceBridge_IsBlockedLanguage(
    JNIEnv* env,
    const JavaParamRef<jstring>& language) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::string language_code(ConvertJavaStringToUTF8(env, language));
  language::ToTranslateLanguageSynonym(&language_code);

  // Application language is always blocked.
  std::string app_locale = g_browser_process->GetApplicationLocale();
  language::ToTranslateLanguageSynonym(&app_locale);
  if (app_locale == language_code)
    return true;

  return translate_prefs->IsBlockedLanguage(language_code);
}

static void JNI_PrefServiceBridge_SetLanguageBlockedState(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jboolean blocked) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (blocked) {
    translate_prefs->BlockLanguage(language_code);
  } else {
    translate_prefs->UnblockLanguage(language_code);
  }
}

static ScopedJavaLocalRef<jstring>
JNI_PrefServiceBridge_GetDownloadDefaultDirectory(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kDownloadDefaultDirectory));
}

static void JNI_PrefServiceBridge_SetDownloadAndSaveFileDefaultDirectory(
    JNIEnv* env,
    const JavaParamRef<jstring>& directory) {
  base::FilePath path(ConvertJavaStringToUTF8(env, directory));
  GetPrefService()->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  GetPrefService()->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}

static jboolean JNI_PrefServiceBridge_GetExplicitLanguageAskPromptShown(
    JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  return translate_prefs->GetExplicitLanguageAskPromptShown();
}

static void JNI_PrefServiceBridge_SetExplicitLanguageAskPromptShown(
    JNIEnv* env,
    jboolean shown) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  translate_prefs->SetExplicitLanguageAskPromptShown(shown);
}

const char* PrefServiceBridge::GetPrefNameExposedToJava(int pref_index) {
  DCHECK_GE(pref_index, 0);
  DCHECK_LT(pref_index, Pref::PREF_NUM_PREFS);
  return kPrefsExposedToJava[pref_index];
}

static void JNI_PrefServiceBridge_SetForceWebContentsDarkModeEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kWebKitForceDarkModeEnabled, enabled);
}
