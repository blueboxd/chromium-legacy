// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
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
    // The access point is only used for metrics, so its value doesn't affect
    // the rest of the tests and can always be the same.
    DeepScanningDialogDelegate::ShowForWebContents(
        web_contents, std::move(data), std::move(callback),
        DeepScanAccessPoint::UPLOAD);
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
};

using DeepScanningDialogDelegateIsEnabledTest = BaseTest;

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMTokenNoPref) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
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
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoPref) {
  DisableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
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
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
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
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_NONE);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
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
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
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
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
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
      profile(), GURL("http://example.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(DO_NOT_SCAN);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref4) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList2) {
  EnableFeatures({kContentComplianceEnabled, kMalwareScanEnabled});
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
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
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
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
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(), url, &data));
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
      profile(), GURL("http://example.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data));
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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good2.doc"));

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
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_NONE);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/enc.zip"));

  SetPathIsEncrypted(data.paths[0]);

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
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_UPLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/enc.zip"));

  SetPathIsEncrypted(data.paths[0]);

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bad.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good2.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bad.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/good.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bad.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.text.emplace_back(base::UTF8ToUTF16("bar"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_1.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_2.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_status.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_rule.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("dlp_text"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_0.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_1.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_2.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_status.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_rule.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

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
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.7z"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.bzip"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.cab"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.docx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.eps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.gzip"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.odt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.pdf"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ppt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.pptx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.rar"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.rtf"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.tar"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.txt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.wpd"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xls"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xlsx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.zip"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(21u, data.paths.size());
                   EXPECT_EQ(21u, result.paths_results.size());

                   // The supported types should be marked as false.
                   for (const auto& result : result.paths_results)
                     EXPECT_FALSE(result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypes) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.these"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.file"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.types"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.are"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.not"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.supported"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
                   EXPECT_EQ(6u, data.paths.size());
                   ASSERT_EQ(6u, result.paths_results.size());

                   // The unsupported types should be marked as true.
                   for (const bool path_result : result.paths_results)
                     EXPECT_TRUE(path_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedAndUnsupportedTypes) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // Only 3 of these file types are supported (bzip, cab and doc). They are
  // mixed in the list so as to show that insertion order does not matter.
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.bzip"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.these"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.file"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.types"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.cab"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.are"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.not"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.supported"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_no_extension"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.unsupported_extension"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/dlp_fail.doc"));

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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.txt"));

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
                    GetParam());
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

}  // namespace

}  // namespace safe_browsing
