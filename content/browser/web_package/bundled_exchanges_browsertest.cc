// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif  // OS_ANDROID

namespace content {
namespace {

base::FilePath GetDefaultTestDataPath() {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("content/test/data/bundled_exchanges/"
                                       "bundled_exchanges_browsertest.wbn")));
}

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool CanAcceptUntrustedExchangesIfNeeded() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserClient);
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  explicit FinishNavigationObserver(WebContents* contents,
                                    base::OnceClosure done_closure)
      : WebContentsObserver(contents), done_closure_(std::move(done_closure)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    error_code_ = navigation_handle->GetNetErrorCode();
    std::move(done_closure_).Run();
  }

  const base::Optional<net::Error>& error_code() const { return error_code_; }

 private:
  base::OnceClosure done_closure_;
  base::Optional<net::Error> error_code_;

  DISALLOW_COPY_AND_ASSIGN(FinishNavigationObserver);
};

}  // namespace

class BundledExchangesTrustableFileBrowserTestBase : public ContentBrowserTest {
 protected:
  BundledExchangesTrustableFileBrowserTestBase() = default;
  ~BundledExchangesTrustableFileBrowserTestBase() override = default;

  void SetUp() override {
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
#if defined(OS_ANDROID)
    // TODO(crbug.com/864403): It seems that we call unsupported Android APIs on
    // KitKat when we set a ContentBrowserClient. Don't call such APIs and make
    // this test available on KitKat.
    int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                                 &bugfix_version);
    if (major_version < 5)
      return;
#endif
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableBundledExchangesFileUrl,
                                    test_data_url().spec());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  void NavigateToBundleAndWaitForReady() {
    base::string16 expected_title = base::ASCIIToUTF16("Ready");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), test_data_url(),
                              GURL("https://test.example.org/")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void RunTestScript(const std::string& script) {
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                              "loadScript('" + script + "');"));

    base::string16 ok = base::ASCIIToUTF16("OK");
    TitleWatcher title_watcher(shell()->web_contents(), ok);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    EXPECT_EQ(ok, title_watcher.WaitAndGetTitle());
  }

  const GURL& test_data_url() const { return test_data_url_; }

  ContentBrowserClient* original_client_ = nullptr;
  GURL test_data_url_;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesTrustableFileBrowserTestBase);
};

enum class TestFilePathMode {
  kNormalFilePath,
#if defined(OS_ANDROID)
  kContentURI,
#endif  // OS_ANDROID
};

class BundledExchangesTrustableFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public BundledExchangesTrustableFileBrowserTestBase {
 protected:
  BundledExchangesTrustableFileBrowserTest() {
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      test_data_url_ = net::FilePathToFileURL(GetDefaultTestDataPath());
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    base::FilePath tmp_dir;
    CHECK(base::GetTempDir(&tmp_dir));
    // The directory name "bundled_exchanges" must be kept in sync with
    // content/shell/android/browsertests_apk/res/xml/file_paths.xml
    base::FilePath tmp_wbn_dir = tmp_dir.AppendASCII("bundled_exchanges");
    CHECK(base::CreateDirectoryAndGetError(tmp_wbn_dir, nullptr));
    base::FilePath temp_file;
    CHECK(base::CreateTemporaryFileInDir(tmp_wbn_dir, &temp_file));
    CHECK(base::CopyFile(GetDefaultTestDataPath(), temp_file));
    test_data_url_ = GURL(base::GetContentUriFromFilePath(temp_file).value());
#endif  // OS_ANDROID
  }
  ~BundledExchangesTrustableFileBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesTrustableFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest,
                       TrustableBundledExchangesFile) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady();
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest, RangeRequest) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady();
  RunTestScript("test-range-request.js");
}

INSTANTIATE_TEST_SUITE_P(BundledExchangesTrustableFileBrowserTests,
                         BundledExchangesTrustableFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

class BundledExchangesTrustableFileNotFoundBrowserTest
    : public BundledExchangesTrustableFileBrowserTestBase {
 protected:
  BundledExchangesTrustableFileNotFoundBrowserTest() {
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_data_url_ =
        net::FilePathToFileURL(test_data_dir.AppendASCII("not_found"));
  }
  ~BundledExchangesTrustableFileNotFoundBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(BundledExchangesTrustableFileNotFoundBrowserTest,
                       NotFound) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), test_data_url(),
                             GURL("https://test.example.org/")));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());
}

}  // namespace content
