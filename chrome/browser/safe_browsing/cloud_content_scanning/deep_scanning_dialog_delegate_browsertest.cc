// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind_test_util.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/ui/browser.h"

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

  int requests_count() const { return requests_count_; }

 private:
  void UploadForDeepScanning(std::unique_ptr<Request> request) override {
    // The first uploaded request is the authentication one.
    if (++requests_count_ == 1) {
      authorization_request_.swap(request);
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
};

FakeBinaryUploadService* FakeBinaryUploadServiceStorage() {
  static FakeBinaryUploadService service;
  return &service;
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

  void EnableUploadScanning() {
    SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));

    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);
    SetWaitPolicy(DELAY_UPLOADS);
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }
};

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Unauthorized) {
  EnableUploadScanning();

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
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath ok_path = temp_dir.GetPath().AppendASCII("ok.doc");
  base::FilePath bad_path = temp_dir.GetPath().AppendASCII("bad.exe");

  base::File ok_file(ok_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File bad_file(bad_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  std::string ok_content = "ok file content";
  std::string bad_content = "bad file content";

  ok_file.WriteAtCurrentPos(ok_content.data(), ok_content.size());
  bad_file.WriteAtCurrentPos(bad_content.data(), bad_content.size());

  // Set up delegate and upload service.
  EnableUploadScanning();

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

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "ok.doc", BinaryUploadService::Result::SUCCESS, ok_response);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "bad.exe", BinaryUploadService::Result::SUCCESS, bad_response);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.paths.emplace_back(ok_path);
  data.paths.emplace_back(bad_path);

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

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request per file and 1 for authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
}

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Texts) {
  // Set up delegate and upload service.
  EnableUploadScanning();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  DeepScanningClientResponse response =
      SimpleDeepScanningClientResponseForTesting(/*dlp=*/false,
                                                 /*malware=*/base::nullopt);
  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);

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
