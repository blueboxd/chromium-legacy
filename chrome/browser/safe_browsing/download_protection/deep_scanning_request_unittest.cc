// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Return;
using ::testing::ReturnRef;

class FakeBinaryUploadService : public BinaryUploadService {
 public:
  FakeBinaryUploadService()
      : BinaryUploadService(/*url_loader_factory=*/nullptr,
                            /*profile=*/nullptr,
                            /*binary_fcm_service=*/nullptr),
        saved_result_(BinaryUploadService::Result::UNKNOWN),
        saved_response_(DeepScanningClientResponse()) {}

  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override {
    last_request_ = request->deep_scanning_request();
    request->FinishRequest(saved_result_, saved_response_);
  }

  void SetResponse(BinaryUploadService::Result result,
                   DeepScanningClientResponse response) {
    saved_result_ = result;
    saved_response_ = response;
  }

  const DeepScanningClientRequest& last_request() { return last_request_; }

 private:
  BinaryUploadService::Result saved_result_;
  DeepScanningClientResponse saved_response_;
  DeepScanningClientRequest last_request_;
};

class FakeDownloadProtectionService : public DownloadProtectionService {
 public:
  FakeDownloadProtectionService() : DownloadProtectionService(nullptr) {}

  void RequestFinished(DeepScanningRequest* request) override {}

  BinaryUploadService* GetBinaryUploadService(Profile* profile) override {
    return &binary_upload_service_;
  }

  FakeBinaryUploadService* GetFakeBinaryUploadService() {
    return &binary_upload_service_;
  }

 private:
  FakeBinaryUploadService binary_upload_service_;
};

class DeepScanningRequestTest : public testing::Test {
 public:
  DeepScanningRequestTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    download_path_ = temp_dir_.GetPath().AppendASCII("download.exe");
    std::string download_contents = "download contents";
    download_hash_ = "hash";
    download_url_ = GURL("https://example.com/download.exe");

    base::File download(download_path_,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    download.WriteAtCurrentPos(download_contents.c_str(),
                               download_contents.size());
    download.Close();

    EXPECT_CALL(item_, GetFullPath()).WillRepeatedly(ReturnRef(download_path_));
    EXPECT_CALL(item_, GetTotalBytes())
        .WillRepeatedly(Return(download_contents.size()));
    EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url_));
    EXPECT_CALL(item_, GetHash()).WillRepeatedly(ReturnRef(download_hash_));
    EXPECT_CALL(item_, GetTargetFilePath())
        .WillRepeatedly(ReturnRef(download_path_));
    EXPECT_CALL(item_, GetMimeType())
        .WillRepeatedly(Return("application/octet-stream"));
    content::DownloadItemUtils::AttachInfo(&item_, profile_, nullptr);

    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));

    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Append("example.com");
  }

  void TearDown() override {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyTokenForTesting());
  }

  void SetDlpPolicy(CheckContentComplianceValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kCheckContentCompliance, state);
  }

  void SetMalwarePolicy(SendFilesForMalwareCheckValues state) {
    profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingSendFilesForMalwareCheck, state);
  }

  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  void DisableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, features);
  }

  void SetLastResult(DownloadCheckResult result) { last_result_ = result; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeDownloadProtectionService download_protection_service_;
  download::MockDownloadItem item_;

  base::ScopedTempDir temp_dir_;
  base::FilePath download_path_;
  GURL download_url_;
  std::string download_hash_;

  DownloadCheckResult last_result_;
};

TEST_F(DeepScanningRequestTest, ChecksFeatureFlags) {
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  {
    EnableFeatures({kMalwareScanEnabled, kContentComplianceEnabled});
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_malware_scan_request());
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_dlp_scan_request());
  }
  {
    DisableFeatures({kMalwareScanEnabled, kContentComplianceEnabled});
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_malware_scan_request());
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_dlp_scan_request());
  }
  {
    EnableFeatures({kContentComplianceEnabled});
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_malware_scan_request());
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_dlp_scan_request());
  }
  {
    EnableFeatures({kMalwareScanEnabled});
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_malware_scan_request());
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_dlp_scan_request());
  }
}

TEST_F(DeepScanningRequestTest, GeneratesCorrectRequestFromPolicy) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});

  {
    SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
    SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_malware_scan_request());
    EXPECT_EQ(download_protection_service_.GetFakeBinaryUploadService()
                  ->last_request()
                  .malware_scan_request()
                  .population(),
              MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_dlp_scan_request());
  }

  {
    SetDlpPolicy(CHECK_NONE);
    SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_malware_scan_request());
    EXPECT_EQ(download_protection_service_.GetFakeBinaryUploadService()
                  ->last_request()
                  .malware_scan_request()
                  .population(),
              MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_dlp_scan_request());
  }

  {
    SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
    SetMalwarePolicy(DO_NOT_SCAN);
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_malware_scan_request());
    EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                    ->last_request()
                    .has_dlp_scan_request());
  }

  {
    SetDlpPolicy(CHECK_NONE);
    SetMalwarePolicy(DO_NOT_SCAN);
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::DoNothing(), &download_protection_service_);
    request.Start();
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_malware_scan_request());
    EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                     ->last_request()
                     .has_dlp_scan_request());
  }
}

TEST_F(DeepScanningRequestTest, GeneratesCorrectRequestForAPP) {
  DeepScanningRequest request(
      &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_APP_PROMPT,
      base::DoNothing(), &download_protection_service_);
  request.Start();

  EXPECT_TRUE(download_protection_service_.GetFakeBinaryUploadService()
                  ->last_request()
                  .has_malware_scan_request());
  EXPECT_FALSE(download_protection_service_.GetFakeBinaryUploadService()
                   ->last_request()
                   .has_dlp_scan_request());
  EXPECT_EQ(download_protection_service_.GetFakeBinaryUploadService()
                ->last_request()
                .malware_scan_request()
                .population(),
            MalwareDeepScanningClientRequest::POPULATION_TITANIUM);
}

TEST_F(DeepScanningRequestTest, ProcessesResponseCorrectly) {
  {
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::BindRepeating(&DeepScanningRequestTest::SetLastResult,
                            base::Unretained(this)),
        &download_protection_service_);

    DeepScanningClientResponse response;
    response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::MALWARE);
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    download_protection_service_.GetFakeBinaryUploadService()->SetResponse(
        BinaryUploadService::Result::SUCCESS, response);

    request.Start();

    EXPECT_EQ(DownloadCheckResult::DANGEROUS, last_result_);
  }

  {
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::BindRepeating(&DeepScanningRequestTest::SetLastResult,
                            base::Unretained(this)),
        &download_protection_service_);

    DeepScanningClientResponse response;
    response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::UWS);
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    download_protection_service_.GetFakeBinaryUploadService()->SetResponse(
        BinaryUploadService::Result::SUCCESS, response);

    request.Start();

    EXPECT_EQ(DownloadCheckResult::POTENTIALLY_UNWANTED, last_result_);
  }

  {
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::BindRepeating(&DeepScanningRequestTest::SetLastResult,
                            base::Unretained(this)),
        &download_protection_service_);

    DeepScanningClientResponse response;
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    download_protection_service_.GetFakeBinaryUploadService()->SetResponse(
        BinaryUploadService::Result::SUCCESS, response);

    request.Start();

    EXPECT_EQ(DownloadCheckResult::SENSITIVE_CONTENT_BLOCK, last_result_);
  }

  {
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::BindRepeating(&DeepScanningRequestTest::SetLastResult,
                            base::Unretained(this)),
        &download_protection_service_);

    DeepScanningClientResponse response;
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::WARN);
    download_protection_service_.GetFakeBinaryUploadService()->SetResponse(
        BinaryUploadService::Result::SUCCESS, response);

    request.Start();

    EXPECT_EQ(DownloadCheckResult::SENSITIVE_CONTENT_WARNING, last_result_);
  }

  {
    DeepScanningRequest request(
        &item_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        base::BindRepeating(&DeepScanningRequestTest::SetLastResult,
                            base::Unretained(this)),
        &download_protection_service_);

    DeepScanningClientResponse response;
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::WARN);
    response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    download_protection_service_.GetFakeBinaryUploadService()->SetResponse(
        BinaryUploadService::Result::SUCCESS, response);

    request.Start();

    EXPECT_EQ(DownloadCheckResult::SENSITIVE_CONTENT_BLOCK, last_result_);
  }
}

}  // namespace safe_browsing
