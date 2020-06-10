// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/PrivacyPreferencesManager_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/util.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"

#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using chrome_browser_net::DnsProbeRunner;

namespace secure_dns = chrome_browser_net::secure_dns;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

net::DohProviderEntry::List GetFilteredProviders() {
  const auto local_providers = secure_dns::ProvidersForCountry(
      net::DohProviderEntry::GetList(), country_codes::GetCurrentCountryID());
  return secure_dns::RemoveDisabledProviders(
      local_providers, secure_dns::GetDisabledProviders());
}

// Runs a DNS probe according to the configuration in |overrides|,
// asynchronously sets |success| to indicate the result, and signals
// |waiter| when the probe has completed.  Must run on the UI thread.
void RunProbe(base::WaitableEvent* waiter,
              bool* success,
              net::DnsConfigOverrides overrides) {
  auto* manager = g_browser_process->system_network_context_manager();
  auto runner = std::make_unique<DnsProbeRunner>(
      std::move(overrides),
      base::BindRepeating(&SystemNetworkContextManager::GetContext,
                          base::Unretained(manager)));
  runner->RunProbe(base::BindOnce(
      [](base::WaitableEvent* waiter, bool* success,
         std::unique_ptr<DnsProbeRunner> runner) {
        *success = runner->result() == DnsProbeRunner::CORRECT;
        waiter->Signal();
      },
      waiter, success, std::move(runner)));
}

}  // namespace

static jboolean JNI_PrivacyPreferencesManager_GetNetworkPredictionEnabled(
    JNIEnv* env) {
  return GetPrefService()->GetInteger(prefs::kNetworkPredictionOptions) !=
         chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

static jboolean JNI_PrivacyPreferencesManager_GetNetworkPredictionManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kNetworkPredictionOptions);
}

static jboolean JNI_PrivacyPreferencesManager_IsMetricsReportingEnabled(
    JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

static void JNI_PrivacyPreferencesManager_SetMetricsReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, enabled);
}

static jboolean JNI_PrivacyPreferencesManager_IsMetricsReportingManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      metrics::prefs::kMetricsReportingEnabled);
}

static jboolean JNI_PrivacyPreferencesManager_CanPrefetchAndPrerender(
    JNIEnv* env) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
         chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static void JNI_PrivacyPreferencesManager_SetNetworkPredictionEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetInteger(
      prefs::kNetworkPredictionOptions,
      enabled ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
              : chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

static jboolean
JNI_PrivacyPreferencesManager_ObsoleteNetworkPredictionOptionsHasUserSetting(
    JNIEnv* env) {
  return GetPrefService()->GetUserPrefValue(prefs::kNetworkPredictionOptions) !=
         nullptr;
}

static jint JNI_PrivacyPreferencesManager_GetSecureDnsMode(JNIEnv* env) {
  return static_cast<int>(
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */)
          .mode());
}

static void JNI_PrivacyPreferencesManager_SetSecureDnsMode(JNIEnv* env,
                                                           jint mode) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::ModeToString(
                             static_cast<net::DnsConfig::SecureDnsMode>(mode)));
}

static jboolean JNI_PrivacyPreferencesManager_IsSecureDnsModeManaged(
    JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->IsManagedPreference(prefs::kDnsOverHttpsMode);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacyPreferencesManager_GetDohProviders(JNIEnv* env) {
  net::DohProviderEntry::List providers = GetFilteredProviders();
  std::vector<std::vector<base::string16>> ret;
  ret.reserve(providers.size());
  std::transform(providers.begin(), providers.end(), std::back_inserter(ret),
                 [](const auto* entry) {
                   return std::vector<base::string16>{
                       {base::UTF8ToUTF16(entry->ui_name),
                        base::UTF8ToUTF16(entry->dns_over_https_template),
                        base::UTF8ToUTF16(entry->privacy_policy)}};
                 });
  return base::android::ToJavaArrayOfStringArray(env, ret);
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacyPreferencesManager_GetDnsOverHttpsTemplates(JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return base::android::ConvertUTF8ToJavaString(
      env, local_state->GetString(prefs::kDnsOverHttpsTemplates));
}

static jboolean JNI_PrivacyPreferencesManager_SetDnsOverHttpsTemplates(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtemplates) {
  PrefService* local_state = g_browser_process->local_state();
  std::string templates = base::android::ConvertJavaStringToUTF8(jtemplates);
  if (templates.empty()) {
    local_state->ClearPref(prefs::kDnsOverHttpsTemplates);
    return true;
  }

  if (secure_dns::IsValidGroup(templates)) {
    local_state->SetString(prefs::kDnsOverHttpsTemplates, templates);
    return true;
  }

  return false;
}

static jint JNI_PrivacyPreferencesManager_GetSecureDnsManagementMode(
    JNIEnv* env) {
  return static_cast<int>(
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */)
          .management_mode());
}

static void JNI_PrivacyPreferencesManager_UpdateDohDropdownHistograms(
    JNIEnv* env,
    const JavaParamRef<jstring>& old_template,
    const JavaParamRef<jstring>& new_template) {
  secure_dns::UpdateDropdownHistograms(
      GetFilteredProviders(),
      base::android::ConvertJavaStringToUTF8(old_template),
      base::android::ConvertJavaStringToUTF8(new_template));
}

static void JNI_PrivacyPreferencesManager_UpdateDohValidationHistogram(
    JNIEnv* env,
    jboolean valid) {
  secure_dns::UpdateValidationHistogram(valid);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacyPreferencesManager_SplitDohTemplateGroup(
    JNIEnv* env,
    const JavaParamRef<jstring>& jgroup) {
  std::string group = base::android::ConvertJavaStringToUTF8(jgroup);
  std::vector<base::StringPiece> templates = secure_dns::SplitGroup(group);
  std::vector<std::string> templates_copy(templates.begin(), templates.end());
  return base::android::ToJavaArrayOfStrings(env, templates_copy);
}

static jboolean JNI_PrivacyPreferencesManager_ProbeDohServer(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtemplate) {
  net::DnsConfigOverrides overrides;
  overrides.search = std::vector<std::string>();
  overrides.attempts = 1;
  overrides.randomize_ports = false;
  overrides.secure_dns_mode = net::DnsConfig::SecureDnsMode::SECURE;
  secure_dns::ApplyTemplate(&overrides,
                            base::android::ConvertJavaStringToUTF8(jtemplate));

  // Android recommends converting async functions to blocking when using JNI:
  // https://developer.android.com/training/articles/perf-jni.
  // This function converts the DnsProbeRunner, which can only be created and
  // used on the UI thread, into a blocking function that can be called from an
  // auxiliary Java thread.
  // TODO: Use std::future if allowed in the future.
  base::WaitableEvent waiter;
  bool success;
  bool posted = content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(RunProbe, &waiter, &success, std::move(overrides)));
  DCHECK(posted);
  waiter.Wait();

  secure_dns::UpdateProbeHistogram(success);
  return success;
}
