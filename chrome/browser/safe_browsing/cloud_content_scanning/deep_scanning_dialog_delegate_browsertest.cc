// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

using extensions::SafeBrowsingPrivateEventRouter;
using ::testing::_;
using ::testing::Mock;

namespace safe_browsing {

namespace {

class FakeBinaryUploadService : public BinaryUploadService {
 public:
  FakeBinaryUploadService() : BinaryUploadService(nullptr, nullptr, nullptr) {}

  // Sets whether the user is authorized to upload data for Deep Scanning.
  void SetAuthorized(bool authorized) {
    authorization_result_ = authorized
                                ? BinaryUploadService::Result::SUCCESS
                                : BinaryUploadService::Result::UNAUTHORIZED;
  }

  // Finish the authentication request. Called after ShowForWebContents to
  // simulate an async callback.
  void ReturnAuthorizedResponse() {
    authorization_request_->FinishRequest(authorization_result_,
                                          DeepScanningClientResponse());
  }

  void SetResponseForText(BinaryUploadService::Result result,
                          const DeepScanningClientResponse& response) {
    prepared_text_result_ = result;
    prepared_text_response_ = response;
  }

  void SetResponseForFile(const std::string& path,
                          BinaryUploadService::Result result,
                          const DeepScanningClientResponse& response) {
    prepared_file_results_[path] = result;
    prepared_file_responses_[path] = response;
  }

  void SetShouldAutomaticallyAuthorize(bool authorize) {
    should_automatically_authorize_ = authorize;
  }

  int requests_count() const { return requests_count_; }

 private:
  void UploadForDeepScanning(std::unique_ptr<Request> request) override {
    // The first uploaded request is the authentication one.
    if (++requests_count_ == 1) {
      authorization_request_.swap(request);

      if (should_automatically_authorize_)
        ReturnAuthorizedResponse();
    } else {
      std::string file = request->deep_scanning_request().filename();
      if (file.empty()) {
        request->FinishRequest(prepared_text_result_, prepared_text_response_);
      } else {
        ASSERT_TRUE(prepared_file_results_.count(file));
        ASSERT_TRUE(prepared_file_responses_.count(file));
        request->FinishRequest(prepared_file_results_[file],
                               prepared_file_responses_[file]);
      }
    }
  }

  BinaryUploadService::Result authorization_result_;
  std::unique_ptr<Request> authorization_request_;
  BinaryUploadService::Result prepared_text_result_;
  DeepScanningClientResponse prepared_text_response_;
  std::map<std::string, BinaryUploadService::Result> prepared_file_results_;
  std::map<std::string, DeepScanningClientResponse> prepared_file_responses_;
  int requests_count_ = 0;
  bool should_automatically_authorize_ = false;
};

FakeBinaryUploadService* FakeBinaryUploadServiceStorage() {
  static FakeBinaryUploadService service;
  return &service;
}

const std::set<std::string>* ExeMimeTypes() {
  static std::set<std::string> set = {"application/x-msdownload",
                                      "application/x-ms-dos-executable",
                                      "application/octet-stream"};
  return &set;
}

const std::set<std::string>* ZipMimeTypes() {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return &set;
}

const std::set<std::string>* ShellScriptMimeTypes() {
  static std::set<std::string> set = {"text/x-sh", "application/x-shellscript"};
  return &set;
}

const std::set<std::string>* TextMimeTypes() {
  static std::set<std::string> set = {"text/plain"};
  return &set;
}

// A fake delegate with minimal overrides to obtain behavior that's as close to
// the real one as possible.
class MinimalFakeDeepScanningDialogDelegate
    : public DeepScanningDialogDelegate {
 public:
  MinimalFakeDeepScanningDialogDelegate(
      content::WebContents* web_contents,
      DeepScanningDialogDelegate::Data data,
      DeepScanningDialogDelegate::CompletionCallback callback)
      : DeepScanningDialogDelegate(web_contents,
                                   std::move(data),
                                   std::move(callback),
                                   DeepScanAccessPoint::UPLOAD) {}

  static std::unique_ptr<DeepScanningDialogDelegate> Create(
      content::WebContents* web_contents,
      DeepScanningDialogDelegate::Data data,
      DeepScanningDialogDelegate::CompletionCallback callback) {
    return std::make_unique<MinimalFakeDeepScanningDialogDelegate>(
        web_contents, std::move(data), std::move(callback));
  }

 private:
  BinaryUploadService* GetBinaryUploadService() override {
    return FakeBinaryUploadServiceStorage();
  }
};

constexpr char kDmToken[] = "dm_token";

std::vector<base::FilePath>* CreatedFilePaths() {
  static std::vector<base::FilePath> paths;
  return &paths;
}

}  // namespace

// Tests the behavior of the dialog delegate with minimal overriding of methods.
// Only responses obtained via the BinaryUploadService are faked.
class DeepScanningDialogDelegateBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver {
 public:
  DeepScanningDialogDelegateBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  ~DeepScanningDialogDelegateBrowserTest() override {
    Mock::VerifyAndClearExpectations(client_.get());
  }

  void EnableUploadsScanningAndReporting() {
    SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));

    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);
    SetWaitPolicy(DELAY_UPLOADS);
    SetUnsafeEventsReportingPolicy(true);

    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetCloudPolicyClientForTesting(client_.get());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetBinaryUploadServiceForTesting(FakeBinaryUploadServiceStorage());
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  void CreateFilesForTest(const std::vector<std::string>& paths,
                          const std::vector<std::string>& contents,
                          DeepScanningDialogDelegate::Data* data) {
    ASSERT_EQ(paths.size(), contents.size());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    for (size_t i = 0; i < paths.size(); ++i) {
      base::FilePath path = temp_dir_.GetPath().AppendASCII(paths[i]);
      CreatedFilePaths()->emplace_back(path);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(contents[i].data(), contents[i].size());
      data->paths.emplace_back(path);
    }
  }

  void ExpectNoReport() {
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);
  }

  void ExpectDangerousDeepScanningResult(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size) {
    expected_event_key_ =
        SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
    expected_url_ = expected_url;
    expected_filename_ = expected_filename;
    expected_sha256_ = expected_sha256;
    expected_threat_type_ = expected_threat_type;
    expected_mimetypes_ = expected_mimetypes;
    expected_trigger_ = expected_trigger;
    expected_content_size_ = expected_content_size;
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
        .WillOnce([this](base::Value& report,
                         base::OnceCallback<void(bool)>& callback) {
          ValidateReport(report);
        });
  }

  void ExpectSensitiveDataEvent(
      const DlpDeepScanningVerdict& expected_dlp_verdict,
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size) {
    expected_event_key_ =
        SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
    expected_url_ = expected_url;
    expected_dlp_verdict_ = expected_dlp_verdict;
    expected_filename_ = expected_filename;
    expected_mimetypes_ = expected_mimetypes;
    expected_trigger_ = expected_trigger;
    expected_clicked_through_ = false;
    expected_content_size_ = expected_content_size;
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
        .WillOnce([this](base::Value& report,
                         base::OnceCallback<void(bool)>& callback) {
          ValidateReport(report);
        });
  }

  void ExpectUnscannedFileEvent(const std::string& expected_url,
                                const std::string& expected_filename,
                                const std::string& expected_sha256,
                                const std::string& expected_trigger,
                                const std::string& expected_reason,
                                const std::set<std::string>* expected_mimetypes,
                                int expected_content_size) {
    expected_event_key_ =
        SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
    expected_url_ = expected_url;
    expected_filename_ = expected_filename;
    expected_sha256_ = expected_sha256;
    expected_mimetypes_ = expected_mimetypes;
    expected_trigger_ = expected_trigger;
    expected_reason_ = expected_reason;
    expected_content_size_ = expected_content_size;
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
        .WillOnce([this](base::Value& report,
                         base::OnceCallback<void(bool)>& callback) {
          ValidateReport(report);
        });
  }

 private:
  void ValidateReport(base::Value& report) {
    // Extract the event list.
    base::Value* event_list = report.FindKey(
        policy::RealtimeReportingJobConfiguration::kEventListKey);
    ASSERT_NE(nullptr, event_list);
    EXPECT_EQ(base::Value::Type::LIST, event_list->type());
    const base::Value::ListView mutable_list = event_list->GetList();

    // There should only be 1 event per test.
    ASSERT_EQ(1, (int)mutable_list.size());
    base::Value wrapper = std::move(mutable_list[0]);
    EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
    value_ = wrapper.FindKey(expected_event_key_);
    ASSERT_NE(nullptr, value_);
    ASSERT_EQ(base::Value::Type::DICTIONARY, value_->type());

    // The event should match the expected values.
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyUrl, expected_url_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyFileName,
                  expected_filename_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256,
                  expected_sha256_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTrigger,
                  expected_trigger_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyContentSize,
                  expected_content_size_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyThreatType,
                  expected_threat_type_);
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyReason, expected_reason_);
    ValidateMimeType();
    ValidateDlpVerdict();
  }

  void ValidateMimeType() {
    std::string* type =
        value_->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyContentType);
    if (expected_mimetypes_)
      EXPECT_TRUE(base::Contains(*expected_mimetypes_, *type));
    else
      EXPECT_EQ(nullptr, type);
  }

  void ValidateDlpVerdict() {
    if (!expected_dlp_verdict_.has_value())
      return;

    ValidateField(SafeBrowsingPrivateEventRouter::kKeyClickedThrough,
                  expected_clicked_through_);
    base::Value* triggered_rules = value_->FindListKey(
        SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
    ASSERT_NE(nullptr, triggered_rules);
    ASSERT_EQ(base::Value::Type::LIST, triggered_rules->type());
    base::Value::ListView rules_list = triggered_rules->GetList();
    int rules_size = rules_list.size();
    ASSERT_EQ(rules_size, expected_dlp_verdict_.value().triggered_rules_size());
    for (int i = 0; i < rules_size; ++i) {
      value_ = &rules_list[i];
      ASSERT_EQ(base::Value::Type::DICTIONARY, value_->type());
      ValidateDlpRule(expected_dlp_verdict_.value().triggered_rules(i));
    }
  }

  void ValidateDlpRule(const DlpDeepScanningVerdict::TriggeredRule& rule) {
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleAction,
                  base::Optional<int>(rule.action()));
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                  rule.rule_name());
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                  base::Optional<int>(rule.rule_id()));
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleSeverity,
                  rule.rule_severity());
    ValidateField(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleResourceName,
                  rule.rule_resource_name());

    base::Value* matched_detectors = value_->FindListKey(
        SafeBrowsingPrivateEventRouter::kKeyMatchedDetectors);
    ASSERT_NE(nullptr, matched_detectors);
    ASSERT_EQ(base::Value::Type::LIST, matched_detectors->type());
    base::Value::ListView detectors_list = matched_detectors->GetList();
    int detectors_size = detectors_list.size();
    ASSERT_EQ(detectors_size, rule.matched_detectors_size());

    for (int j = 0; j < detectors_size; ++j) {
      value_ = &detectors_list[j];
      ASSERT_EQ(base::Value::Type::DICTIONARY, value_->type());
      const DlpDeepScanningVerdict::MatchedDetector& detector =
          rule.matched_detectors(j);
      ValidateField(SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorId,
                    detector.detector_id());
      ValidateField(SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorName,
                    detector.display_name());
      ValidateField(SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorType,
                    detector.detector_type());
    }
  }

  void ValidateField(const std::string& field_key,
                     const base::Optional<std::string>& expected_value) {
    if (expected_value.has_value())
      ASSERT_EQ(*value_->FindStringKey(field_key), expected_value.value());
    else
      ASSERT_EQ(nullptr, value_->FindStringKey(field_key));
  }

  void ValidateField(const std::string& field_key,
                     const base::Optional<int>& expected_value) {
    ASSERT_EQ(value_->FindIntKey(field_key), expected_value);
  }

  void ValidateField(const std::string& field_key,
                     const base::Optional<bool>& expected_value) {
    ASSERT_EQ(value_->FindBoolKey(field_key), expected_value);
  }

  // Expected fields for reports checked against |value_|. The optional ones are
  // not present on every unsafe event. The mimetype field is handled by a
  // pointer to a set since different builds/platforms can return different
  // mimetype strings for the same file.
  std::string expected_event_key_;
  std::string expected_url_;
  std::string expected_filename_;
  std::string expected_sha256_;
  std::string expected_trigger_;
  base::Optional<DlpDeepScanningVerdict> expected_dlp_verdict_ = base::nullopt;
  base::Optional<std::string> expected_threat_type_ = base::nullopt;
  base::Optional<std::string> expected_reason_ = base::nullopt;
  base::Optional<bool> expected_clicked_through_ = base::nullopt;
  base::Optional<int> expected_content_size_ = base::nullopt;
  const std::set<std::string>* expected_mimetypes_ = nullptr;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  base::ScopedTempDir temp_dir_;
  base::Value* value_;
};

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Unauthorized) {
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(false);

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

  // Nothing should be reported for unauthorized users.
  ExpectNoReport();

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](
              const DeepScanningDialogDelegate::Data& data,
              const DeepScanningDialogDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            ASSERT_TRUE(result.paths_results[0]);
            called = true;
            quit_closure.Run();
          }),
      DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // Only 1 request (the authentication one) should have been uploaded.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create the files to be opened and scanned.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  CreateFilesForTest({"ok.doc", "bad.exe"},
                     {"ok file content", "bad file content"}, &data);

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  DeepScanningClientResponse ok_response;
  ok_response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  ok_response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::CLEAN);

  DeepScanningClientResponse bad_response;
  bad_response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  bad_response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::MALWARE);

  // The malware verdict means an event should be reported.
  ExpectDangerousDeepScanningResult(
      /*url*/ "about:blank",
      /*filename*/ CreatedFilePaths()->at(1).AsUTF8Unsafe(),
      // printf "bad file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "77AE96C38386429D28E53F5005C46C7B4D8D39BE73D757CE61E0AE65CC1A5A5D",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ std::string("bad file content").size());

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "ok.doc", BinaryUploadService::Result::SUCCESS, ok_response);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "bad.exe", BinaryUploadService::Result::SUCCESS, bad_response);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_FALSE(result.paths_results[1]);
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request per file and 1 for authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
}

class DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<AllowPasswordProtectedFilesValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  AllowPasswordProtectedFilesValues allow_password_protected_files() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (allow_password_protected_files()) {
      case ALLOW_NONE:
      case ALLOW_DOWNLOADS:
        return false;
      case ALLOW_UPLOADS:
      case ALLOW_UPLOADS_AND_DOWNLOADS:
        return true;
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetAllowPasswordProtectedFilesPolicy(allow_password_protected_files());

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.paths.emplace_back(test_zip);

  // The file should be reported as unscanned.
  ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ test_zip.AsUTF8Unsafe(),
      // TODO(1061461): Check SHA256 in this test once the bug is fixed.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "filePasswordProtected",
      /*mimetypes*/ ZipMimeTypes(),
      // TODO(1061461): Put real size once the file contents are read.
      /*size*/ 0);

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication needed to report the unscanned file
  // event.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    testing::Values(ALLOW_NONE,
                    ALLOW_DOWNLOADS,
                    ALLOW_UPLOADS,
                    ALLOW_UPLOADS_AND_DOWNLOADS));

class DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<BlockUnsupportedFiletypesValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  BlockUnsupportedFiletypesValues block_unsupported_file_types() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (GetParam()) {
      case BLOCK_UNSUPPORTED_FILETYPES_NONE:
      case BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS:
        return true;
      case BLOCK_UNSUPPORTED_FILETYPES_UPLOADS:
      case BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS:
        return false;
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create the files with unsupported types.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = false;
  CreateFilesForTest({"a.sh"}, {"file content"}, &data);

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetBlockUnsupportedFileTypesPolicy(block_unsupported_file_types());

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // The file should be reported as unscanned.
  ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ CreatedFilePaths()->at(0).AsUTF8Unsafe(),
      // TODO(1061461): Check SHA256 in this test once the bug is fixed.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "unsupportedFileType",
      /*mimetype*/ ShellScriptMimeTypes(),
      // TODO(1061461): Put real size once the file contents are read.
      /*size*/ 0);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication needed to report the unscanned file
  // event.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    testing::Values(BLOCK_UNSUPPORTED_FILETYPES_NONE,
                    BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS,
                    BLOCK_UNSUPPORTED_FILETYPES_UPLOADS,
                    BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS));

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Texts) {
  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  // Prepare a complex DLP response to test that the verdict is reported
  // correctly in the sensitive data event.
  DeepScanningClientResponse response;
  DlpDeepScanningVerdict* verdict = response.mutable_dlp_scan_verdict();
  verdict->set_status(DlpDeepScanningVerdict::SUCCESS);

  DlpDeepScanningVerdict::TriggeredRule* rule1 = verdict->add_triggered_rules();
  rule1->set_rule_id(1);
  rule1->set_action(DlpDeepScanningVerdict::TriggeredRule::REPORT_ONLY);
  rule1->set_rule_resource_name("resource name 1");
  rule1->set_rule_severity("severity 1");
  DlpDeepScanningVerdict::MatchedDetector* detector1 =
      rule1->add_matched_detectors();
  detector1->set_detector_id("id1");
  detector1->set_detector_type("dlp1");
  detector1->set_display_name("display name 1");

  DlpDeepScanningVerdict::TriggeredRule* rule2 = verdict->add_triggered_rules();
  rule2->set_rule_id(3);
  rule2->set_action(DlpDeepScanningVerdict::TriggeredRule::BLOCK);
  rule2->set_rule_resource_name("resource rule 2");
  rule2->set_rule_severity("severity 2");
  DlpDeepScanningVerdict::MatchedDetector* detector2_1 =
      rule2->add_matched_detectors();
  detector2_1->set_detector_id("id2.1");
  detector2_1->set_detector_type("type2.1");
  detector2_1->set_display_name("display name 2.1");
  DlpDeepScanningVerdict::MatchedDetector* detector2_2 =
      rule2->add_matched_detectors();
  detector2_2->set_detector_id("id2.2");
  detector2_2->set_detector_type("type2.2");
  detector2_2->set_display_name("display name 2.2");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);

  // The DLP verdict means an event should be reported. The content size is
  // equal to the length of the concatenated texts ("text1" and "text2") times 2
  // since they are wide characters ((5 + 5) * 2 = 20).
  ExpectSensitiveDataEvent(
      /* dlp_verdict*/ response.dlp_scan_verdict(),
      /*url*/ "about:blank",
      /*filename*/ "Text data",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 20);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.text.emplace_back(base::UTF8ToUTF16("text1"));
  data.text.emplace_back(base::UTF8ToUTF16("text2"));

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_FALSE(result.text_results[0]);
            ASSERT_FALSE(result.text_results[1]);
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for all texts and 1 for authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

}  // namespace safe_browsing
