// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com/";

constexpr char kTestHttpsSchemePatternUrl[] = "https://*";
constexpr char kTestChromeSchemePatternUrl[] = "chrome://*";
constexpr char kTestDevtoolsSchemePatternUrl[] = "devtools://*";

constexpr char kTestPathPatternUrl[] = "*/a/specific/path/";
constexpr char kTestPortPatternUrl[] = "*:1234";
constexpr char kTestQueryPatternUrl[] = "*?q=5678";

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyTokenForTesting());
  }
};

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    DeepScanningDialogDelegate::DisableUIForTesting();
  }

  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  void DisableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, features);
  }

  void SetDlpPolicy(CheckContentComplianceValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kCheckContentCompliance, state);
  }

  void SetWaitPolicy(DelayDeliveryUntilVerdictValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kDelayDeliveryUntilVerdict, state);
  }

  void SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kAllowPasswordProtectedFiles, state);
  }

  void SetMalwarePolicy(SendFilesForMalwareCheckValues state) {
    profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingSendFilesForMalwareCheck, state);
  }

  void SetBlockLargeFilePolicy(BlockLargeFileTransferValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kBlockLargeFileTransfer, state);
  }

  void AddUrlToList(const char* pref_name, const GURL& url) {
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(), pref_name)
        ->Append(url.host());
  }

  void AddUrlToList(const char* pref_name, const char* url) {
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(), pref_name)
        ->Append(url);
  }

  void ScanUpload(content::WebContents* web_contents,
                  DeepScanningDialogDelegate::Data data,
                  DeepScanningDialogDelegate::CompletionCallback callback) {
    // The access point is only used for metrics and choosing the dialog text if
    // one is shown, so its value doesn't affect the tests in this file and can
    // always be the same.
    DeepScanningDialogDelegate::ShowForWebContents(
        web_contents, std::move(data), std::move(callback),
        DeepScanAccessPoint::UPLOAD);
  }

  void CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      DeepScanningDialogDelegate::Data* data) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos("content", 7);
      data->paths.emplace_back(path);
    }
  }

  void SetUp() override {
    // Always set this so DeepScanningDialogDelegate::ShowForWebContents waits
    // for the verdict before running its callback.
    SetWaitPolicy(DELAY_UPLOADS);
  }

  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  base::ScopedTempDir temp_dir_;
};

}  // namespace

using DeepScanningDialogDelegateIsEnabledTest = BaseTest;

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMTokenNoPref) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMToken) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoPref) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMToken) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeature) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_NONE);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabledWithUrl) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  GURL url(kTestUrl);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_EQ(kTestUrl, data.url);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByList) {
  GURL url(kTestUrl);
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByListWithPatterns) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent, kTestUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestChromeSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestDevtoolsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestPathPatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestPortPatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestQueryPatternUrl);

  DeepScanningDialogDelegate::Data data;

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://example.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(DO_NOT_SCAN);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref4) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabled) {
  GURL url(kTestUrl);
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoScanInIncognito) {
  GURL url(kTestUrl);
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabledWithPatterns) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, kTestUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestChromeSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestDevtoolsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestPathPatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestPortPatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestQueryPatternUrl);

  DeepScanningDialogDelegate::Data data;

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://example.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

class DeepScanningDialogDelegateAuditOnlyTest : public BaseTest {
 protected:
  void RunUntilDone() { run_loop_.Run(); }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile());
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void SetDLPResponse(DlpDeepScanningVerdict verdict) {
    dlp_verdict_ = verdict;
  }

  void PathFailsDeepScan(base::FilePath path,
                         DeepScanningClientResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

  void SetPathIsEncrypted(base::FilePath path) {
    encrypted_.insert(std::move(path));
  }

  void SetScanPolicies(bool dlp, bool malware) {
    include_dlp_ = dlp;
    include_malware_ = malware;

    if (include_dlp_)
      SetDlpPolicy(CHECK_UPLOADS);
    else
      SetDlpPolicy(CHECK_NONE);

    if (include_malware_)
      SetMalwarePolicy(SEND_UPLOADS);
    else
      SetMalwarePolicy(DO_NOT_SCAN);
  }

 private:
  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &DeepScanningDialogDelegateAuditOnlyTest::StatusCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &DeepScanningDialogDelegateAuditOnlyTest::EncryptionStatusCallback,
            base::Unretained(this)),
        kDmToken));
  }

  DeepScanningClientResponse StatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the |failures_| maps.
    auto it = failures_.find(path);
    DeepScanningClientResponse response =
        it != failures_.end()
            ? it->second
            : FakeDeepScanningDialogDelegate::SuccessfulResponse(
                  include_dlp_, include_malware_);

    if (include_dlp_ && dlp_verdict_.has_value())
      *response.mutable_dlp_scan_verdict() = dlp_verdict_.value();

    return response;
  }

  bool EncryptionStatusCallback(const base::FilePath& path) {
    return encrypted_.count(path) > 0;
  }

  base::RunLoop run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};
  bool include_dlp_ = true;
  bool include_malware_ = true;

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, DeepScanningClientResponse> failures_;

  // Paths in this set will be considered to contain encryption and will
  // not be uploaded.
  std::set<base::FilePath> encrypted_;

  // DLP response to ovewrite in the callback if present.
  base::Optional<DlpDeepScanningVerdict> dlp_verdict_ = base::nullopt;
};

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // Keep |data| empty by not setting any text or paths.

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.text.emplace_back(base::UTF8ToUTF16("bar"));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.text_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_NONE);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_FALSE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_UPLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataNegativeMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::MALWARE));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileDataPositiveDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileDataNegativeDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::SUCCESS, "rule",
                        DlpDeepScanningVerdict::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataNegativeMalwareAndDlpVerdicts) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(
      data.paths[1],
      FakeDeepScanningDialogDelegate::MalwareAndDlpResponse(
          MalwareDeepScanningVerdict::MALWARE, DlpDeepScanningVerdict::SUCCESS,
          "rule", DlpDeepScanningVerdict::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataNoDLP) {
  // Enable malware scan so deep scanning still occurs.
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.text.emplace_back(base::UTF8ToUTF16("bar"));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_FALSE(result.text_results[0]);
                   EXPECT_FALSE(result.text_results[1]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataFailedDLP) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("good"));
  data.text.emplace_back(base::UTF8ToUTF16("bad"));

  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
                     DlpDeepScanningVerdict::SUCCESS, "rule",
                     DlpDeepScanningVerdict::TriggeredRule::BLOCK)
                     .dlp_scan_verdict());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   ASSERT_EQ(0u, result.paths_results.size());
                   EXPECT_FALSE(result.text_results[0]);
                   EXPECT_FALSE(result.text_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark some files with failed scans.
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::UWS));
  PathFailsDeepScan(data.paths[2],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::MALWARE));
  PathFailsDeepScan(data.paths[3],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::FAILURE, "",
                        DlpDeepScanningVerdict::TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::SUCCESS, "rule",
                        DlpDeepScanningVerdict::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(5u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   ASSERT_EQ(5u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   EXPECT_FALSE(result.paths_results[2]);
                   EXPECT_FALSE(result.paths_results[3]);
                   EXPECT_FALSE(result.paths_results[4]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, NoDelay) {
  SetWaitPolicy(DELAY_NONE);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  data.text.emplace_back(base::UTF8ToUTF16("dlp_text"));
  CreateFilesForTest({FILE_PATH_LITERAL("foo_fail_malware_0.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark all files and text with failed scans.
  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
                     DlpDeepScanningVerdict::SUCCESS, "rule",
                     DlpDeepScanningVerdict::TriggeredRule::BLOCK)
                     .dlp_scan_verdict());
  PathFailsDeepScan(data.paths[0],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::MALWARE));
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::UWS));
  PathFailsDeepScan(data.paths[2],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::MALWARE));
  PathFailsDeepScan(data.paths[3],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::FAILURE, "",
                        DlpDeepScanningVerdict::TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::SUCCESS, "rule",
                        DlpDeepScanningVerdict::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(5u, data.paths.size());
                   EXPECT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(5u, result.paths_results.size());

                   // All results are set to true since we are not blocking the
                   // user.
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   EXPECT_TRUE(result.paths_results[2]);
                   EXPECT_TRUE(result.paths_results[3]);
                   EXPECT_TRUE(result.paths_results[4]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, EmptyWait) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedTypes) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  std::vector<base::FilePath::StringType> file_names;
  for (const base::FilePath::StringType& supported_type :
       SupportedDlpFileTypes()) {
    file_names.push_back(base::FilePath::StringType(FILE_PATH_LITERAL("foo")) +
                         supported_type);
  }
  CreateFilesForTest(file_names, &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(24u, data.paths.size());
                   EXPECT_EQ(24u, result.paths_results.size());

                   // The supported types should be marked as false.
                   for (const auto& result : result.paths_results)
                     EXPECT_FALSE(result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypesDefaultPolicy) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(6u, data.paths.size());
                   ASSERT_EQ(6u, result.paths_results.size());

                   // The unsupported types should be marked as true since the
                   // default policy behavior is to allow them through.
                   for (const bool path_result : result.paths_results)
                     EXPECT_TRUE(path_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypesBlockPolicy) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
      prefs::kBlockUnsupportedFiletypes, BLOCK_UNSUPPORTED_FILETYPES_UPLOADS);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(6u, data.paths.size());
                   ASSERT_EQ(6u, result.paths_results.size());

                   // The unsupported types should be marked as false since the
                   // block policy behavior is to not allow them through.
                   for (const bool path_result : result.paths_results)
                     EXPECT_FALSE(path_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedAndUnsupportedTypes) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // Only 3 of these file types are supported (bzip, cab and doc). They are
  // mixed in the list so as to show that insertion order does not matter.
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.bzip"), FILE_PATH_LITERAL("foo.these"),
       FILE_PATH_LITERAL("foo.file"), FILE_PATH_LITERAL("foo.types"),
       FILE_PATH_LITERAL("foo.cab"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported"),
       FILE_PATH_LITERAL("foo_no_extension"), FILE_PATH_LITERAL("foo.doc")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));
  }

  bool called = false;
  ScanUpload(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(10u, data.paths.size());
            ASSERT_EQ(10u, result.paths_results.size());

            // The unsupported types should be marked as true, and the valid
            // types as false since they are marked as failed scans.
            size_t i = 0;
            for (const bool expected : {false, true, true, true, false, true,
                                        true, true, true, false}) {
              ASSERT_EQ(expected, result.paths_results[i]);
              ++i;
            }
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypeAndDLPFailure) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.unsupported_extension"),
                      FILE_PATH_LITERAL("dlp_fail.doc")},
                     &data);

  // Mark DLP as failure.
  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
                     DlpDeepScanningVerdict::SUCCESS, "rule",
                     DlpDeepScanningVerdict::TriggeredRule::BLOCK)
                     .dlp_scan_verdict());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());

                   // The unsupported type file should be marked as true, and
                   // the valid type file as false.
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

// TODO(crbug/1041890): This test should not depend on
// DeepScanningDialogDelegateAuditOnlyTest. Refactor this by moving common
// functionalities to BaseTest or util classes/functions.
class DeepScanningDialogDelegateResultHandlingTest
    : public DeepScanningDialogDelegateAuditOnlyTest,
      public testing::WithParamInterface<BinaryUploadService::Result> {};

TEST_P(DeepScanningDialogDelegateResultHandlingTest, Test) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  FakeDeepScanningDialogDelegate::SetResponseResult(GetParam());
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.txt")}, &data);

  bool called = false;
  ScanUpload(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(0u, data.text.size());
            EXPECT_EQ(1u, data.paths.size());
            EXPECT_EQ(0u, result.text_results.size());
            EXPECT_EQ(1u, result.paths_results.size());

            bool expected =
                DeepScanningDialogDelegate::ResultShouldAllowDataUse(
                    GetParam(), data.settings);
            EXPECT_EQ(expected, result.paths_results[0]);
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(
    Tests,
    DeepScanningDialogDelegateResultHandlingTest,
    testing::Values(BinaryUploadService::Result::UNKNOWN,
                    BinaryUploadService::Result::SUCCESS,
                    BinaryUploadService::Result::UPLOAD_FAILURE,
                    BinaryUploadService::Result::TIMEOUT,
                    BinaryUploadService::Result::FILE_TOO_LARGE,
                    BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
                    BinaryUploadService::Result::UNAUTHORIZED,
                    BinaryUploadService::Result::FILE_ENCRYPTED));

class DeepScanningDialogDelegatePolicyResultsTest : public BaseTest {
 public:
  enterprise_connectors::AnalysisSettings settings() {
    base::Optional<enterprise_connectors::AnalysisSettings> settings =
        enterprise_connectors::ConnectorsManager::GetInstance()
            ->GetAnalysisSettings(
                GURL(kTestUrl),
                enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

TEST_F(DeepScanningDialogDelegatePolicyResultsTest, BlockLargeFile) {
  // The value returned by ResultShouldAllowDataUse for FILE_TOO_LARGE should
  // match the BlockLargeFilePolicy.
  SetBlockLargeFilePolicy(
      BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_LARGE_DOWNLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_NONE);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));
}

TEST_F(DeepScanningDialogDelegatePolicyResultsTest,
       AllowPasswordProtectedFiles) {
  // The value returned by ResultShouldAllowDataUse for FILE_ENCRYPTED should
  // match the AllowPasswordProtectedFiles policy.
  SetAllowPasswordPolicy(
      AllowPasswordProtectedFilesValues::ALLOW_UPLOADS_AND_DOWNLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_DOWNLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_UPLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_NONE);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));
}

}  // namespace safe_browsing
