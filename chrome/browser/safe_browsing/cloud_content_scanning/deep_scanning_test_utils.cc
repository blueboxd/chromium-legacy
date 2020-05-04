// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::SafeBrowsingPrivateEventRouter;
using ::testing::_;

namespace safe_browsing {

namespace {

base::Value MakeListValue(const std::vector<std::string>& elements) {
  base::Value list(base::Value::Type::LIST);
  for (const std::string& element : elements)
    list.Append(element);
  return list;
}

base::Value DefaultConnectorSettings() {
  base::Value settings(base::Value::Type::DICTIONARY);

  settings.SetKey(enterprise_connectors::kKeyEnable,
                  base::Value(base::Value::Type::LIST));
  settings.SetKey(enterprise_connectors::kKeyDisable,
                  base::Value(base::Value::Type::LIST));

  return settings;
}

void InitConnectorPrefIfEmpty(
    enterprise_connectors::AnalysisConnector connector) {
  ListPrefUpdate settings_list(g_browser_process->local_state(),
                               ConnectorPref(connector));
  DCHECK(settings_list.Get());
  if (settings_list->empty())
    settings_list->Append(DefaultConnectorSettings());
}

void AddConnectorUrlPattern(enterprise_connectors::AnalysisConnector connector,
                            bool enable,
                            base::Value url_list,
                            base::Value tags) {
  InitConnectorPrefIfEmpty(connector);

  ListPrefUpdate settings_list(g_browser_process->local_state(),
                               ConnectorPref(connector));
  base::Value* settings = nullptr;
  DCHECK(settings_list->Get(0, &settings));
  DCHECK(settings);
  DCHECK(settings->is_dict());

  base::Value* list =
      settings->FindListPath(enable ? enterprise_connectors::kKeyEnable
                                    : enterprise_connectors::kKeyDisable);
  DCHECK(list);

  base::Value list_element(base::Value::Type::DICTIONARY);
  list_element.SetKey(enterprise_connectors::kKeyUrlList, std::move(url_list));
  list_element.SetKey(enterprise_connectors::kKeyTags, std::move(tags));

  list->Append(std::move(list_element));
}

void SetConnectorField(enterprise_connectors::AnalysisConnector connector,
                       const char* key,
                       bool value) {
  InitConnectorPrefIfEmpty(connector);
  ListPrefUpdate settings_list(g_browser_process->local_state(),
                               ConnectorPref(connector));
  base::Value* settings = nullptr;
  DCHECK(settings_list->Get(0, &settings));
  DCHECK(settings);
  DCHECK(settings->is_dict());
  settings->SetKey(key, base::Value(std::move(value)));
}

}  // namespace

EventReportValidator::EventReportValidator(
    policy::MockCloudPolicyClient* client)
    : client_(client) {}

EventReportValidator::~EventReportValidator() {
  testing::Mock::VerifyAndClearExpectations(client_);
}

void EventReportValidator::ExpectUnscannedFileEvent(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const std::string& expected_reason,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
  filename_ = expected_filename;
  sha256_ = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  reason_ = expected_reason;
  content_size_ = expected_content_size;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce([this](base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectDangerousDeepScanningResult(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_threat_type,
    const std::string& expected_trigger,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filename_ = expected_filename;
  sha256_ = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce([this](base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectSensitiveDataEvent(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const DlpDeepScanningVerdict& expected_dlp_verdict,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  dlp_verdict_ = expected_dlp_verdict;
  filename_ = expected_filename;
  sha256_ = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  clicked_through_ = false;
  content_size_ = expected_content_size;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce([this](base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::
    ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
        const std::string& expected_url,
        const std::string& expected_filename,
        const std::string& expected_sha256,
        const std::string& expected_threat_type,
        const std::string& expected_trigger,
        const DlpDeepScanningVerdict& expected_dlp_verdict,
        const std::set<std::string>* expected_mimetypes,
        int expected_content_size) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filename_ = expected_filename;
  sha256_ = expected_sha256;
  threat_type_ = expected_threat_type;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce([this](base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      })
      .WillOnce(
          [this, expected_dlp_verdict](
              base::Value& report, base::OnceCallback<void(bool)>& callback) {
            event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
            threat_type_ = base::nullopt;
            clicked_through_ = false;
            dlp_verdict_ = expected_dlp_verdict;
            ValidateReport(&report);
            if (!done_closure_.is_null())
              done_closure_.Run();
          });
}

void EventReportValidator::ValidateReport(base::Value* report) {
  DCHECK(report);

  // Extract the event list.
  base::Value* event_list =
      report->FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  const base::Value::ListView mutable_list = event_list->GetList();

  // There should only be 1 event per test.
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(event_key_);
  ASSERT_NE(nullptr, event);
  ASSERT_EQ(base::Value::Type::DICTIONARY, event->type());

  // The event should match the expected values.
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUrl, url_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyFileName, filename_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256,
                sha256_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyTrigger, trigger_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyContentSize,
                content_size_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyThreatType,
                threat_type_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyReason, reason_);
  ValidateMimeType(event);
  ValidateDlpVerdict(event);
}

void EventReportValidator::ValidateMimeType(base::Value* value) {
  std::string* type =
      value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyContentType);
  if (mimetypes_)
    EXPECT_TRUE(base::Contains(*mimetypes_, *type));
  else
    EXPECT_EQ(nullptr, type);
}

void EventReportValidator::ValidateDlpVerdict(base::Value* value) {
  if (!dlp_verdict_.has_value())
    return;

  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyClickedThrough,
                clicked_through_);
  base::Value* triggered_rules =
      value->FindListKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rules);
  ASSERT_EQ(base::Value::Type::LIST, triggered_rules->type());
  base::Value::ListView rules_list = triggered_rules->GetList();
  int rules_size = rules_list.size();
  ASSERT_EQ(rules_size, dlp_verdict_.value().triggered_rules_size());
  for (int i = 0; i < rules_size; ++i) {
    base::Value* rule = &rules_list[i];
    ASSERT_EQ(base::Value::Type::DICTIONARY, rule->type());
    ValidateDlpRule(rule, dlp_verdict_.value().triggered_rules(i));
  }
}

void EventReportValidator::ValidateDlpRule(
    base::Value* value,
    const DlpDeepScanningVerdict::TriggeredRule& expected_rule) {
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleAction,
                base::Optional<int>(expected_rule.action()));
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                expected_rule.rule_name());
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                base::Optional<int>(expected_rule.rule_id()));
  ValidateField(value,
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleSeverity,
                expected_rule.rule_severity());
  ValidateField(value,
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleResourceName,
                expected_rule.rule_resource_name());

  base::Value* matched_detectors =
      value->FindListKey(SafeBrowsingPrivateEventRouter::kKeyMatchedDetectors);
  ASSERT_NE(nullptr, matched_detectors);
  ASSERT_EQ(base::Value::Type::LIST, matched_detectors->type());
  base::Value::ListView detectors_list = matched_detectors->GetList();
  int detectors_size = detectors_list.size();
  ASSERT_EQ(detectors_size, expected_rule.matched_detectors_size());

  for (int j = 0; j < detectors_size; ++j) {
    base::Value* detector = &detectors_list[j];
    ASSERT_EQ(base::Value::Type::DICTIONARY, detector->type());
    const DlpDeepScanningVerdict::MatchedDetector& expected_detector =
        expected_rule.matched_detectors(j);
    ValidateField(detector,
                  SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorId,
                  expected_detector.detector_id());
    ValidateField(detector,
                  SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorName,
                  expected_detector.display_name());
    ValidateField(detector,
                  SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorType,
                  expected_detector.detector_type());
  }
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<std::string>& expected_value) {
  if (expected_value.has_value())
    ASSERT_EQ(*value->FindStringKey(field_key), expected_value.value());
  else
    ASSERT_EQ(nullptr, value->FindStringKey(field_key));
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<int>& expected_value) {
  ASSERT_EQ(value->FindIntKey(field_key), expected_value);
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<bool>& expected_value) {
  ASSERT_EQ(value->FindBoolKey(field_key), expected_value);
}

void EventReportValidator::SetDoneClosure(base::RepeatingClosure closure) {
  done_closure_ = std::move(closure);
}

void SetDlpPolicyForConnectors(CheckContentComplianceValues state) {
  // The legacy DLP policy has the following behavior:
  // - On uploads, scan everything for DLP if it's enabled unless the URL
  //   matches kURLsToNotCheckComplianceOfUploadedContent, and scan nothing if
  //   it is disabled.
  // - On downloads, only scan URLs matching
  //   kURLsToCheckComplianceOfDownloadedContent if it's enabled, otherwise scan
  //   nothing for DLP.

  // This is replicated in the connector policies by adding the wildcard pattern
  // on upload connectors with the "dlp" tag in "enable" or "disable". The
  // wildcard pattern should also be included in the disable list if the policy
  // is disabled for downloads since no scan can occur with the legacy policy
  // when it is disabled.

  bool enable_uploads =
      state == CHECK_UPLOADS || state == CHECK_UPLOADS_AND_DOWNLOADS;

  AddConnectorUrlPattern(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED, enable_uploads,
      MakeListValue({"*"}), MakeListValue({"dlp"}));
  AddConnectorUrlPattern(
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY, enable_uploads,
      MakeListValue({"*"}), MakeListValue({"dlp"}));

  if (state != CHECK_DOWNLOADS && state != CHECK_UPLOADS_AND_DOWNLOADS) {
    AddConnectorUrlPattern(
        enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED, false,
        MakeListValue({"*"}), MakeListValue({"dlp"}));
  }
}

void SetMalwarePolicyForConnectors(SendFilesForMalwareCheckValues state) {
  // The legacy Malware policy has the following behavior:
  // - On uploads, only scan URLs matching
  //   kURLsToCheckForMalwareOfUploadedContent if it's enabled, otherwise scan
  //   nothing for malware.
  // - On downloard, scan everything for malware if it's enabled unless the URL
  //   matches kURLsToNotCheckForMalwareOfDownloadedContent, and scan nothing if
  //   it's disabled.

  // This is replicated in the connector policies by adding the wildcard pattern
  // on the download connector with the "malware" tag in "enable" or "disable".
  // The wildcard pattern should also be included in the disable list if the
  // policy is disabled for uploads since no scan can occur with the legacy
  // policy when it is disabled.

  bool enable_downloads =
      state == SEND_DOWNLOADS || state == SEND_UPLOADS_AND_DOWNLOADS;

  AddConnectorUrlPattern(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
      enable_downloads, MakeListValue({"*"}), MakeListValue({"malware"}));

  if (state != SEND_UPLOADS && state != SEND_UPLOADS_AND_DOWNLOADS) {
    AddConnectorUrlPattern(
        enterprise_connectors::AnalysisConnector::FILE_ATTACHED, false,
        MakeListValue({"*"}), MakeListValue({"malware"}));
    AddConnectorUrlPattern(
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY, false,
        MakeListValue({"*"}), MakeListValue({"malware"}));
  }
}

void SetDelayDeliveryUntilVerdictPolicyForConnectors(
    DelayDeliveryUntilVerdictValues state) {
  bool delay_uploads =
      state == DELAY_UPLOADS || state == DELAY_UPLOADS_AND_DOWNLOADS;
  bool delay_downloads =
      state == DELAY_DOWNLOADS || state == DELAY_UPLOADS_AND_DOWNLOADS;
  SetConnectorField(enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
                    enterprise_connectors::kKeyBlockUntilVerdict,
                    delay_uploads);
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
                    enterprise_connectors::kKeyBlockUntilVerdict,
                    delay_uploads);
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
                    enterprise_connectors::kKeyBlockUntilVerdict,
                    delay_downloads);
}

void SetAllowPasswordProtectedFilesPolicyForConnectors(
    AllowPasswordProtectedFilesValues state) {
  bool allow_uploads =
      state == ALLOW_UPLOADS || state == ALLOW_UPLOADS_AND_DOWNLOADS;
  bool allow_downloads =
      state == ALLOW_DOWNLOADS || state == ALLOW_UPLOADS_AND_DOWNLOADS;
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
                    enterprise_connectors::kKeyBlockPasswordProtected,
                    allow_uploads);
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
                    enterprise_connectors::kKeyBlockPasswordProtected,
                    allow_downloads);
}

void SetBlockUnsupportedFileTypesPolicyForConnectors(
    BlockUnsupportedFiletypesValues state) {
  bool block_uploads =
      state == BLOCK_UNSUPPORTED_FILETYPES_UPLOADS ||
      state == BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS;
  bool block_downloads =
      state == BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS ||
      state == BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS;
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
                    enterprise_connectors::kKeyBlockUnsupportedFileTypes,
                    block_uploads);
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
                    enterprise_connectors::kKeyBlockUnsupportedFileTypes,
                    block_downloads);
}

void SetBlockLargeFileTransferPolicyForConnectors(
    BlockLargeFileTransferValues state) {
  bool block_uploads = state == BLOCK_LARGE_UPLOADS ||
                       state == BLOCK_LARGE_UPLOADS_AND_DOWNLOADS;
  bool block_downloads = state == BLOCK_LARGE_DOWNLOADS ||
                         state == BLOCK_LARGE_UPLOADS_AND_DOWNLOADS;
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
                    enterprise_connectors::kKeyBlockLargeFiles, block_uploads);
  SetConnectorField(enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
                    enterprise_connectors::kKeyBlockLargeFiles,
                    block_downloads);
}

void AddUrlsToCheckComplianceOfDownloadsForConnectors(
    const std::vector<std::string>& urls) {
  AddConnectorUrlPattern(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED, true,
      MakeListValue(urls), MakeListValue({"dlp"}));
}

void AddUrlsToNotCheckComplianceOfUploadsForConnectors(
    const std::vector<std::string>& urls) {
  for (auto connector :
       {enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY}) {
    AddConnectorUrlPattern(connector, false, MakeListValue(urls),
                           MakeListValue({"dlp"}));
  }
}

void AddUrlsToCheckForMalwareOfUploadsForConnectors(
    const std::vector<std::string>& urls) {
  for (auto connector :
       {enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY}) {
    AddConnectorUrlPattern(connector, true, MakeListValue(urls),
                           MakeListValue({"malware"}));
  }
}

void AddUrlsToNotCheckForMalwareOfDownloadsForConnectors(
    const std::vector<std::string>& urls) {
  AddConnectorUrlPattern(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED, false,
      MakeListValue(urls), MakeListValue({"malware"}));
}

}  // namespace safe_browsing
