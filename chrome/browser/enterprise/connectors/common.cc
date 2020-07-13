// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"

namespace enterprise_connectors {

AnalysisSettings::AnalysisSettings() = default;
AnalysisSettings::AnalysisSettings(AnalysisSettings&&) = default;
AnalysisSettings& AnalysisSettings::operator=(AnalysisSettings&&) = default;
AnalysisSettings::~AnalysisSettings() = default;

ReportingSettings::ReportingSettings() = default;
ReportingSettings::ReportingSettings(GURL url) : reporting_url(url) {}
ReportingSettings::ReportingSettings(ReportingSettings&&) = default;
ReportingSettings& ReportingSettings::operator=(ReportingSettings&&) = default;
ReportingSettings::~ReportingSettings() = default;

const char* ConnectorPref(AnalysisConnector connector) {
  switch (connector) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      return kOnBulkDataEntryPref;
    case AnalysisConnector::FILE_DOWNLOADED:
      return kOnFileDownloadedPref;
    case AnalysisConnector::FILE_ATTACHED:
      return kOnFileAttachedPref;
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED() << "Using unspecified analysis connector";
      return "";
  }
}

const char* ConnectorPref(ReportingConnector connector) {
  switch (connector) {
    case ReportingConnector::SECURITY_EVENT:
      return kOnSecurityEventPref;
  }
}

ContentAnalysisResponse::Result::TriggeredRule::Action
GetHighestPrecedenceAction(const ContentAnalysisResponse& response) {
  auto action =
      ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED;

  for (const auto& result : response.results()) {
    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    for (const auto& rule : result.triggered_rules())
      action = GetHighestPrecedenceAction(action, rule.action());
  }
  return action;
}

ContentAnalysisResponse::Result::TriggeredRule::Action
GetHighestPrecedenceAction(
    const ContentAnalysisResponse::Result::TriggeredRule::Action& action_1,
    const ContentAnalysisResponse::Result::TriggeredRule::Action& action_2) {
  // Don't use the enum's int values to determine precedence since that
  // may introduce bugs for new actions later.
  //
  // The current precedence is BLOCK > WARN > REPORT_ONLY > UNSPECIFIED
  if (action_1 == ContentAnalysisResponse::Result::TriggeredRule::BLOCK ||
      action_2 == ContentAnalysisResponse::Result::TriggeredRule::BLOCK) {
    return ContentAnalysisResponse::Result::TriggeredRule::BLOCK;
  }
  if (action_1 == ContentAnalysisResponse::Result::TriggeredRule::WARN ||
      action_2 == ContentAnalysisResponse::Result::TriggeredRule::WARN) {
    return ContentAnalysisResponse::Result::TriggeredRule::WARN;
  }
  if (action_1 == ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY ||
      action_2 == ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY) {
    return ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY;
  }
  if (action_1 ==
          ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED ||
      action_2 ==
          ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED) {
    return ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED;
  }
  NOTREACHED();
  return ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED;
}

}  // namespace enterprise_connectors
