// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
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

// A derivative of DeepScanningDialogDelegate that overrides calls to the
// real binary upload service and re-implements them locally.
class FakeDeepScanningDialogDelegate : public DeepScanningDialogDelegate {
 public:
  static std::unique_ptr<DeepScanningDialogDelegate> Create(
      base::RepeatingClosure delete_closure,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
        delete_closure, web_contents, std::move(data), std::move(callback));
    return ret;
  }

  FakeDeepScanningDialogDelegate(base::RepeatingClosure delete_closure,
                                 content::WebContents* web_contents,
                                 Data data,
                                 CompletionCallback callback)
      : DeepScanningDialogDelegate(web_contents,
                                   std::move(data),
                                   std::move(callback)),
        delete_closure_(delete_closure) {}

  ~FakeDeepScanningDialogDelegate() override {
    if (!delete_closure_.is_null())
      delete_closure_.Run();
  }

 private:
  void Response(base::FilePath path,
                std::unique_ptr<BinaryUploadService::Request> request) {
    DeepScanningClientResponse response;
    response.mutable_dlp_scan_verdict();
    response.mutable_malware_scan_verdict();

    if (path.empty()) {
      StringRequestCallback(BinaryUploadService::Result::SUCCESS, response);
    } else {
      FileRequestCallback(path, BinaryUploadService::Result::SUCCESS, response);
    }
  }

  void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request) override {
    EXPECT_EQ(
        DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
        request->deep_scanning_request().dlp_scan_request().content_source());
    EXPECT_EQ(kDmToken, request->deep_scanning_request().dm_token());

    // Simulate a response.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                                  base::Unretained(this), base::FilePath(),
                                  std::move(request)));
  }

  void UploadFileForDeepScanning(
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request) override {
    DCHECK(!path.empty());
    EXPECT_EQ(
        DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
        request->deep_scanning_request().dlp_scan_request().content_source());
    EXPECT_EQ(kDmToken, request->deep_scanning_request().dm_token());

    // Simulate a response.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                       base::Unretained(this), path, std::move(request)));
  }

  bool CloseTabModalDialog() override { return false; }

  base::RepeatingClosure delete_closure_;
};

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

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref3) {
  EnableFeatures({kDeepScanningOfUploads});
  SetDMToken(kDmToken);
  SetMalwarePolicy(SEND_FILES_DISABLED);

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

 private:
  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures({kDeepScanningOfUploads});
    SetDMToken(kDmToken);
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure()));
  }

  base::RunLoop run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // Keep |data| empty by not setting any text or paths.

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(0u, data.text.size());
        EXPECT_EQ(0u, data.paths.size());
        EXPECT_EQ(0u, result.text_results.size());
        EXPECT_EQ(0u, result.paths_results.size());
      }));
  RunUntilDone();
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(1u, data.text.size());
        EXPECT_EQ(0u, data.paths.size());
        ASSERT_EQ(1u, result.text_results.size());
        EXPECT_EQ(0u, result.paths_results.size());
        EXPECT_TRUE(result.text_results[0]);
      }));
  RunUntilDone();
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.text.emplace_back(base::UTF8ToUTF16("bar"));

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(2u, data.text.size());
        EXPECT_EQ(0u, data.paths.size());
        ASSERT_EQ(2u, result.text_results.size());
        EXPECT_EQ(0u, result.paths_results.size());
        EXPECT_TRUE(result.text_results[0]);
        EXPECT_TRUE(result.text_results[1]);
      }));
  RunUntilDone();
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(0u, data.text.size());
        EXPECT_EQ(1u, data.paths.size());
        EXPECT_EQ(0u, result.text_results.size());
        ASSERT_EQ(1u, result.paths_results.size());
        EXPECT_TRUE(result.paths_results[0]);
      }));
  RunUntilDone();
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar"));

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(0u, data.text.size());
        EXPECT_EQ(2u, data.paths.size());
        EXPECT_EQ(0u, result.text_results.size());
        ASSERT_EQ(2u, result.paths_results.size());
        EXPECT_TRUE(result.paths_results[0]);
        EXPECT_TRUE(result.paths_results[1]);
      }));
  RunUntilDone();
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar"));

  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce([](const DeepScanningDialogDelegate::Data& data,
                        const DeepScanningDialogDelegate::Result& result) {
        EXPECT_EQ(1u, data.text.size());
        EXPECT_EQ(2u, data.paths.size());
        ASSERT_EQ(1u, result.text_results.size());
        ASSERT_EQ(2u, result.paths_results.size());
        EXPECT_TRUE(result.text_results[0]);
        EXPECT_TRUE(result.paths_results[0]);
        EXPECT_TRUE(result.paths_results[1]);
      }));
  RunUntilDone();
}

}  // namespace

}  // namespace safe_browsing
