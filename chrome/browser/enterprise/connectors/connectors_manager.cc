// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

const base::Feature kEnterpriseConnectorsEnabled{
    "EnterpriseConnectorsEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

base::ListValue AllPatterns() {
  auto v = std::vector<base::Value>();
  v.emplace_back("*");
  return base::ListValue(std::move(v));
}

bool MatchURLAgainstPatterns(const GURL& url,
                             const base::ListValue* patterns_to_scan,
                             const base::ListValue* patterns_to_exempt) {
  url_matcher::URLMatcher matcher;
  url_matcher::URLMatcherConditionSet::ID id(0);

  policy::url_util::AddFilters(&matcher, true, &id, patterns_to_scan);

  url_matcher::URLMatcherConditionSet::ID last_allowed_id = id;

  policy::url_util::AddFilters(&matcher, false, &id, patterns_to_exempt);

  auto matches = matcher.MatchURL(url);
  bool has_scan_match = false;
  for (const auto& match_id : matches) {
    if (match_id <= last_allowed_id)
      has_scan_match = true;
    else
      return false;
  }
  return has_scan_match;
}

}  // namespace

// ConnectorsManager implementation---------------------------------------------
ConnectorsManager::ConnectorsManager() {
  StartObservingPrefs();
}

ConnectorsManager::~ConnectorsManager() = default;

// static
ConnectorsManager* ConnectorsManager::GetInstance() {
  return base::Singleton<ConnectorsManager>::get();
}

bool ConnectorsManager::IsConnectorEnabled(AnalysisConnector connector) {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  if (connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && g_browser_process->local_state()->HasPrefPath(pref);
}

base::Optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  // Prioritize new Connector policies over legacy ones.
  if (IsConnectorEnabled(connector))
    return GetAnalysisSettingsFromConnectorPolicy(url, connector);

  return GetAnalysisSettingsFromLegacyPolicies(url, connector);
}

base::Optional<AnalysisSettings>
ConnectorsManager::GetAnalysisSettingsFromConnectorPolicy(
    const GURL& url,
    AnalysisConnector connector) {
  if (connector_settings_.count(connector) == 0)
    CacheConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (connector_settings_.count(connector) == 0)
    return base::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return connector_settings_[connector][0].GetAnalysisSettings(url);
}

void ConnectorsManager::CacheConnectorPolicy(AnalysisConnector connector) {
  connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      g_browser_process->local_state()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      connector_settings_[connector].emplace_back(service_settings);
  }
}

bool ConnectorsManager::DelayUntilVerdict(AnalysisConnector connector) const {
  bool upload = connector != AnalysisConnector::FILE_DOWNLOADED;
  return LegacyBlockUntilVerdict(upload) == BlockUntilVerdict::BLOCK;
}

base::Optional<AnalysisSettings>
ConnectorsManager::GetAnalysisSettingsFromLegacyPolicies(
    const GURL& url,
    AnalysisConnector connector) const {
  // Legacy policies map to upload/download instead of individual hooks.
  bool upload = connector != AnalysisConnector::FILE_DOWNLOADED;

  std::set<std::string> tags = MatchURLAgainstLegacyPolicies(url, upload);

  // No tags implies the legacy patterns policies didn't match the URL, so no
  // settings are returned.
  if (tags.empty())
    return base::nullopt;

  auto settings = AnalysisSettings();
  settings.block_until_verdict = LegacyBlockUntilVerdict(upload);
  settings.block_password_protected_files =
      LegacyBlockPasswordProtectedFiles(upload);
  settings.block_large_files = LegacyBlockLargeFiles(upload);
  settings.block_unsupported_file_types =
      LegacyBlockUnsupportedFileTypes(upload);
  settings.tags = std::move(tags);

  return settings;
}

BlockUntilVerdict ConnectorsManager::LegacyBlockUntilVerdict(
    bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kDelayDeliveryUntilVerdict);
  if (pref == safe_browsing::DELAY_NONE)
    return BlockUntilVerdict::NO_BLOCK;
  if (pref == safe_browsing::DELAY_UPLOADS_AND_DOWNLOADS)
    return BlockUntilVerdict::BLOCK;
  return ((upload && pref == safe_browsing::DELAY_UPLOADS) ||
          (!upload && pref == safe_browsing::DELAY_DOWNLOADS))
             ? BlockUntilVerdict::BLOCK
             : BlockUntilVerdict::NO_BLOCK;
}

bool ConnectorsManager::LegacyBlockPasswordProtectedFiles(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kAllowPasswordProtectedFiles);
  if (pref == safe_browsing::ALLOW_NONE)
    return true;
  if (pref == safe_browsing::ALLOW_UPLOADS_AND_DOWNLOADS)
    return false;
  return upload ? pref != safe_browsing::ALLOW_UPLOADS
                : pref != safe_browsing::ALLOW_DOWNLOADS;
}

bool ConnectorsManager::LegacyBlockLargeFiles(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kBlockLargeFileTransfer);
  if (pref == safe_browsing::BLOCK_NONE)
    return false;
  if (pref == safe_browsing::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS)
    return true;
  return upload ? pref == safe_browsing::BLOCK_LARGE_UPLOADS
                : pref == safe_browsing::BLOCK_LARGE_DOWNLOADS;
}

bool ConnectorsManager::LegacyBlockUnsupportedFileTypes(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kBlockUnsupportedFiletypes);
  if (pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_NONE)
    return false;
  if (pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS)
    return true;
  return upload ? pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS
                : pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS;
}

bool ConnectorsManager::MatchURLAgainstLegacyDlpPolicies(const GURL& url,
                                                         bool upload) const {
  const base::ListValue all_patterns = AllPatterns();
  const base::ListValue no_patterns = base::ListValue();

  const base::ListValue* patterns_to_scan;
  const base::ListValue* patterns_to_exempt;
  if (upload) {
    patterns_to_scan = &all_patterns;
    patterns_to_exempt = g_browser_process->local_state()->GetList(
        prefs::kURLsToNotCheckComplianceOfUploadedContent);
  } else {
    patterns_to_scan = g_browser_process->local_state()->GetList(
        prefs::kURLsToCheckComplianceOfDownloadedContent);
    patterns_to_exempt = &no_patterns;
  }

  return MatchURLAgainstPatterns(url, patterns_to_scan, patterns_to_exempt);
}

bool ConnectorsManager::MatchURLAgainstLegacyMalwarePolicies(
    const GURL& url,
    bool upload) const {
  const base::ListValue all_patterns = AllPatterns();
  const base::ListValue no_patterns = base::ListValue();

  const base::ListValue* patterns_to_scan;
  const base::ListValue* patterns_to_exempt;
  if (upload) {
    patterns_to_scan = g_browser_process->local_state()->GetList(
        prefs::kURLsToCheckForMalwareOfUploadedContent);
    patterns_to_exempt = &no_patterns;
  } else {
    patterns_to_scan = &all_patterns;
    patterns_to_exempt = g_browser_process->local_state()->GetList(
        prefs::kURLsToNotCheckForMalwareOfDownloadedContent);
  }

  return MatchURLAgainstPatterns(url, patterns_to_scan, patterns_to_exempt);
}

std::set<std::string> ConnectorsManager::MatchURLAgainstLegacyPolicies(
    const GURL& url,
    bool upload) const {
  std::set<std::string> tags;

  if (MatchURLAgainstLegacyDlpPolicies(url, upload))
    tags.emplace("dlp");

  if (MatchURLAgainstLegacyMalwarePolicies(url, upload))
    tags.emplace("malware");

  return tags;
}

void ConnectorsManager::StartObservingPrefs() {
  pref_change_registrar_.Init(g_browser_process->local_state());
  if (base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled)) {
    StartObservingPref(AnalysisConnector::FILE_ATTACHED);
    StartObservingPref(AnalysisConnector::FILE_DOWNLOADED);
    StartObservingPref(AnalysisConnector::BULK_DATA_ENTRY);
  }
}

void ConnectorsManager::StartObservingPref(AnalysisConnector connector) {
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(&ConnectorsManager::CacheConnectorPolicy,
                                  base::Unretained(this), connector));
  }
}

const ConnectorsManager::AnalysisConnectorsSettings&
ConnectorsManager::GetAnalysisConnectorsSettingsForTesting() const {
  return connector_settings_;
}

void ConnectorsManager::SetUpForTesting() {
  StartObservingPrefs();
}

void ConnectorsManager::TearDownForTesting() {
  pref_change_registrar_.RemoveAll();
  connector_settings_.clear();
}

}  // namespace enterprise_connectors
