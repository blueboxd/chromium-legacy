// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

const char kDmToken[] = "dm_token";
const char kTestUrl[] = "http://example.com";

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetDMToken(const char* dm_token) {
    DeepScanningDialogDelegate::SetDMTokenForTesting(dm_token);
  }

  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(features,
                                          std::vector<base::Feature>());
  }

  void SetDlpPolicy(CheckContentComplianceValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kCheckContentCompliance, state);
  }

  void SetWaitPolicy(DelayDeliveryUntilVerdictValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kDelayDeliveryUntilVerdict, state);
  }

  void SetMalwarePolicy(SendFilesForMalwareCheckValues state) {
    profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingSendFilesForMalwareCheck, state);
  }

  void AddUrlToList(const char* pref_name, const GURL& url) {
    ListPrefUpdate updater(TestingBrowserProcess::GetGlobal()->local_state(),
                           pref_name);
    updater->GetList().emplace_back(url.host());
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
  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeatures({kDeepScanningOfUploads});

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetDlpPolicy(CHECK_NONE);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetDlpPolicy(CHECK_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetDlpPolicy(CHECK_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByList) {
  GURL url(kTestUrl);
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kDomainsToNotCheckComplianceOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(DO_NOT_SCAN);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref4) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(SEND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(SEND_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList2) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabled) {
  GURL url(kTestUrl);
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kDomainsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
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

  void PathFailsDeepScan(base::FilePath path,
                         DeepScanningClientResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

 private:
  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures({kDeepScanningOfUploads});
    SetDMToken(kDmToken);
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure(),
        base::Bind(&DeepScanningDialogDelegateAuditOnlyTest::StatusCallback,
                   base::Unretained(this)),
        kDmToken));
  }

  DeepScanningClientResponse StatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the |failures_| maps.
    auto it = failures_.find(path);
    return it != failures_.end()
               ? it->second
               : FakeDeepScanningDialogDelegate::SuccessfulResponse();
  }

  base::RunLoop run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, DeepScanningClientResponse> failures_;
};

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // Keep |data| empty by not setting any text or paths.

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_1"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_2"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_status"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_rule"));

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
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, EmptyWait) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

}  // namespace

}  // namespace safe_browsing
