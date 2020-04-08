// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include <memory>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

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
ConnectorsManager::~ConnectorsManager() = default;

ConnectorsManager::ConnectorsManager() = default;

// static
ConnectorsManager* ConnectorsManager::GetInstance() {
  return base::Singleton<ConnectorsManager>::get();
}

void ConnectorsManager::GetAnalysisSettings(const GURL& url,
                                            AnalysisConnector connector,
                                            AnalysisSettingsCallback callback) {
  std::move(callback).Run(
      GetAnalysisSettingsFromLegacyPolicies(url, connector));
}

base::Optional<ConnectorsManager::AnalysisSettings>
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

// ConnectorsManager structs implementation-------------------------------------

ConnectorsManager::AnalysisSettings::AnalysisSettings() = default;
ConnectorsManager::AnalysisSettings::AnalysisSettings(
    ConnectorsManager::AnalysisSettings&&) = default;
ConnectorsManager::AnalysisSettings&
ConnectorsManager::AnalysisSettings::operator=(
    ConnectorsManager::AnalysisSettings&&) = default;
ConnectorsManager::AnalysisSettings::~AnalysisSettings() = default;

ConnectorsManager::ReportingSettings::ReportingSettings() = default;
ConnectorsManager::ReportingSettings::ReportingSettings(
    ConnectorsManager::ReportingSettings&&) = default;
ConnectorsManager::ReportingSettings&
ConnectorsManager::ReportingSettings::operator=(
    ConnectorsManager::ReportingSettings&&) = default;
ConnectorsManager::ReportingSettings::~ReportingSettings() = default;

}  // namespace enterprise_connectors
