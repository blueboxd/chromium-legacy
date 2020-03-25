// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::SafeBrowsingPrivateEventRouter;
using ::testing::_;

namespace safe_browsing {

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

}  // namespace safe_browsing
