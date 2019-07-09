// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "net/base/url_util.h"

namespace previews {

namespace {

// The group of client-side previews experiments. This controls paramters of the
// client side blacklist.
const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";

// Name for the version parameter of a field trial. Version changes will
// result in older blacklist entries being removed.
const char kVersion[] = "version";

// The threshold of EffectiveConnectionType above which previews will not be
// served.
// See net/nqe/effective_connection_type.h for mapping from string to value.
const char kEffectiveConnectionTypeThreshold[] =
    "max_allowed_effective_connection_type";

// The session maximum threshold of EffectiveConnectionType above which previews
// will not be served. This is maximum limit on top of any per-preview-type
// threshold or per-page-pattern slow page trigger threshold. It is intended
// to be Finch configured on a session basis to limit slow page triggering to
// be a proportion of all eligible page loads.
// See net/nqe/effective_connection_type.h for mapping from string to value.
const char kSessionMaxECTTrigger[] = "session_max_ect_trigger";

// Inflation parameters for estimating NoScript data savings.
const char kNoScriptInflationPercent[] = "NoScriptInflationPercent";
const char kNoScriptInflationBytes[] = "NoScriptInflationBytes";


// Inflation parameters for estimating ResourceLoadingHints data savings.
const char kResourceLoadingHintsInflationPercent[] =
    "ResourceLoadingHintsInflationPercent";
const char kResourceLoadingHintsInflationBytes[] =
    "ResourceLoadingHintsInflationBytes";

size_t GetParamValueAsSizeT(const std::string& trial_name,
                            const std::string& param_name,
                            size_t default_value) {
  size_t value;
  if (!base::StringToSizeT(
          base::GetFieldTrialParamValue(trial_name, param_name), &value)) {
    return default_value;
  }
  return value;
}

int GetParamValueAsInt(const std::string& trial_name,
                       const std::string& param_name,
                       int default_value) {
  int value;
  if (!base::StringToInt(base::GetFieldTrialParamValue(trial_name, param_name),
                         &value)) {
    return default_value;
  }
  return value;
}

net::EffectiveConnectionType GetParamValueAsECT(
    const std::string& trial_name,
    const std::string& param_name,
    net::EffectiveConnectionType default_value) {
  return net::GetEffectiveConnectionTypeForName(
             base::GetFieldTrialParamValue(trial_name, param_name))
      .value_or(default_value);
}

net::EffectiveConnectionType GetParamValueAsECTByFeature(
    const base::Feature& feature,
    const std::string& param_name,
    net::EffectiveConnectionType default_value) {
  return net::GetEffectiveConnectionTypeForName(
             base::GetFieldTrialParamValueByFeature(feature, param_name))
      .value_or(default_value);
}

}  // namespace

namespace params {

size_t MaxStoredHistoryLengthForPerHostBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "per_host_max_stored_history_length", 4);
}

size_t MaxStoredHistoryLengthForHostIndifferentBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "host_indifferent_max_stored_history_length", 10);
}

size_t MaxInMemoryHostsInBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "max_hosts_in_blacklist", 100);
}

int PerHostBlackListOptOutThreshold() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                            "per_host_opt_out_threshold", 2);
}

int HostIndifferentBlackListOptOutThreshold() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                            "host_indifferent_opt_out_threshold", 6);
}

base::TimeDelta PerHostBlackListDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "per_host_black_list_duration_in_days", 30));
}

base::TimeDelta HostIndifferentBlackListPerHostDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "host_indifferent_black_list_duration_in_days", 30));
}

base::TimeDelta SingleOptOutDuration() {
  return base::TimeDelta::FromSeconds(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "single_opt_out_duration_in_seconds", 60 * 5));
}

base::TimeDelta OfflinePreviewFreshnessDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "offline_preview_freshness_duration_in_days", 7));
}

base::TimeDelta LitePagePreviewsSingleBypassDuration() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, "single_bypass_duration_in_seconds",
      60 * 5));
}

base::TimeDelta LitePagePreviewsNavigationTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(features::kLitePageServerPreviews,
                                             "navigation_timeout_milliseconds",
                                             30 * 1000));
}

int LitePageRedirectPreviewMaxServerBlacklistByteSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, "max_blacklist_byte_size",
      250 * 1024 /* 250KB */);
}

size_t LitePageRedirectPreviewMaxNavigationRestarts() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, "max_navigation_restart", 5);
}

int PreviewServerLoadshedMaxSeconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, "loadshed_max_seconds",
      5 * 60 /* 5 minutes */);
}

bool LitePagePreviewsTriggerOnLocalhost() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews, "trigger_on_localhost", false);
}

bool LitePagePreviewsOverridePageHints() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLitePageRedirectOverridesPageHints)) {
    return true;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews, "override_pagehints", false);
}

GURL GetLitePagePreviewsDomainURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLitePageServerPreviewHost)) {
    const std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kLitePageServerPreviewHost);
    const GURL url(switch_value);
    if (url.is_valid())
      return url;
    LOG(WARNING)
        << "The following litepages previews host URL specified at the "
        << "command-line is invalid: " << switch_value;
  }

  std::string variable_host_str = GetFieldTrialParamValueByFeature(
      features::kLitePageServerPreviews, "previews_host");
  if (!variable_host_str.empty()) {
    GURL variable_host(variable_host_str);
    DCHECK(variable_host.is_valid());
    DCHECK(variable_host.has_scheme());
    return variable_host;
  }
  return GURL("https://litepages.googlezip.net/");
}

bool IsInLitePageRedirectControl() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews, "control_group", false);
}

bool LitePageRedirectPreviewShouldPresolve() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews, "preresolve_on_slow_connections",
      true);
}

base::TimeDelta LitePageRedirectPreviewPresolveInterval() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, "preresolve_interval_in_seconds", 60));
}

net::EffectiveConnectionType GetECTThresholdForPreview(
    previews::PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
    case PreviewsType::NOSCRIPT:
    case PreviewsType::LITE_PAGE_REDIRECT:
      return GetParamValueAsECT(kClientSidePreviewsFieldTrial,
                                kEffectiveConnectionTypeThreshold,
                                net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::LITE_PAGE:
      NOTREACHED();
      break;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return GetParamValueAsECTByFeature(features::kResourceLoadingHints,
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::DEFER_ALL_SCRIPT:
      return GetParamValueAsECTByFeature(features::kDeferAllScriptPreviews,
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
}

net::EffectiveConnectionType GetSessionMaxECTThreshold() {
  return GetParamValueAsECTByFeature(features::kSlowPageTriggering,
                                     kSessionMaxECTTrigger,
                                     net::EFFECTIVE_CONNECTION_TYPE_3G);
}

bool ArePreviewsAllowed() {
  return base::FeatureList::IsEnabled(features::kPreviews);
}

bool IsOfflinePreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kOfflinePreviews);
}

bool IsNoScriptPreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kNoScriptPreviews);
}

bool IsResourceLoadingHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kResourceLoadingHints);
}

bool IsLitePageServerPreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kLitePageServerPreviews);
}

bool IsDeferAllScriptPreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kDeferAllScriptPreviews);
}

int OfflinePreviewsVersion() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial, kVersion, 0);
}

int LitePageServerPreviewsVersion() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kLitePageServerPreviews, kVersion, 0);
}

int NoScriptPreviewsVersion() {
  return GetFieldTrialParamByFeatureAsInt(features::kNoScriptPreviews, kVersion,
                                          0);
}

int ResourceLoadingHintsVersion() {
  return GetFieldTrialParamByFeatureAsInt(features::kResourceLoadingHints,
                                          kVersion, 0);
}

int DeferAllScriptPreviewsVersion() {
  return GetFieldTrialParamByFeatureAsInt(features::kDeferAllScriptPreviews,
                                          kVersion, 0);
}

int NoScriptPreviewsInflationPercent() {
  // The default value was determined from lab experiment data of whitelisted
  // URLs. It may be improved once there is enough UKM live experiment data
  // via the field trial param.
  return GetFieldTrialParamByFeatureAsInt(features::kNoScriptPreviews,
                                          kNoScriptInflationPercent, 80);
}

int NoScriptPreviewsInflationBytes() {
  return GetFieldTrialParamByFeatureAsInt(features::kNoScriptPreviews,
                                          kNoScriptInflationBytes, 0);
}

int ResourceLoadingHintsPreviewsInflationPercent() {
  return GetFieldTrialParamByFeatureAsInt(features::kResourceLoadingHints,
                                          kResourceLoadingHintsInflationPercent,
                                          20);
}

int ResourceLoadingHintsPreviewsInflationBytes() {
  return GetFieldTrialParamByFeatureAsInt(
      features::kResourceLoadingHints, kResourceLoadingHintsInflationBytes, 0);
}

size_t OfflinePreviewsHelperMaxPrefSize() {
  return GetFieldTrialParamByFeatureAsInt(
      features::kOfflinePreviewsFalsePositivePrevention, "max_pref_entries",
      100);
}

bool ShouldOverrideNavigationCoinFlipToHoldback() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kCoinFlipHoldback, "force_coin_flip_always_holdback", false);
}

bool ShouldOverrideNavigationCoinFlipToAllowed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kCoinFlipHoldback, "force_coin_flip_always_allow", false);
}

bool ShouldExcludeMediaSuffix(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kExcludedMediaSuffixes))
    return false;

  std::vector<std::string> suffixes = {
      ".apk", ".avi",  ".gif", ".gifv", ".jpeg", ".jpg", ".mp3",
      ".mp4", ".mpeg", ".pdf", ".png",  ".webm", ".webp"};

  std::string csv = base::GetFieldTrialParamValueByFeature(
      features::kExcludedMediaSuffixes, "excluded_path_suffixes");
  if (csv != "") {
    suffixes = base::SplitString(csv, ",", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  }

  for (const std::string& suffix : suffixes) {
    if (base::EndsWith(url.path(), suffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

}  // namespace params

std::string GetStringNameForType(PreviewsType type) {
  // The returned string is used to record histograms for the new preview type.
  // Also add the string to Previews.Types histogram suffix in histograms.xml.
  switch (type) {
    case PreviewsType::NONE:
      return "None";
    case PreviewsType::OFFLINE:
      return "Offline";
    case PreviewsType::LITE_PAGE:
      return "LitePage";
    case PreviewsType::LITE_PAGE_REDIRECT:
      return "LitePageRedirect";
    case PreviewsType::NOSCRIPT:
      return "NoScript";
    case PreviewsType::UNSPECIFIED:
      return "Unspecified";
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return "ResourceLoadingHints";
    case PreviewsType::DEFER_ALL_SCRIPT:
      return "DeferAllScript";
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace previews
