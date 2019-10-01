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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
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

// "%2F" is treated as an invalid character for file URLs.
constexpr char kInvalidFileUrl[] = "file:///tmp/test%2F/a.wbn";

constexpr char kTestPageUrl[] = "https://test.example.org/";
constexpr char kTestPage1Url[] = "https://test.example.org/page1.html";
constexpr char kTestPage2Url[] = "https://test.example.org/page2.html";
constexpr char kTestPageForHashUrl[] =
    "https://test.example.org/hash.html#hello";

base::FilePath GetTestDataPath(base::StringPiece file) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir
      .Append(base::FilePath(
          FILE_PATH_LITERAL("content/test/data/bundled_exchanges")))
      .AppendASCII(file);
}

#if defined(OS_ANDROID)
GURL CopyFileAndGetContentUri(const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath tmp_dir;
  CHECK(base::GetTempDir(&tmp_dir));
  // The directory name "bundled_exchanges" must be kept in sync with
  // content/shell/android/browsertests_apk/res/xml/file_paths.xml
  base::FilePath tmp_wbn_dir = tmp_dir.AppendASCII("bundled_exchanges");
  CHECK(base::CreateDirectoryAndGetError(tmp_wbn_dir, nullptr));
  base::FilePath tmp_dir_in_tmp_wbn_dir;
  CHECK(
      base::CreateTemporaryDirInDir(tmp_wbn_dir, "", &tmp_dir_in_tmp_wbn_dir));
  base::FilePath temp_file = tmp_dir_in_tmp_wbn_dir.Append(file.BaseName());
  CHECK(base::CopyFile(file, temp_file));
  return GURL(base::GetContentUriFromFilePath(temp_file).value());
}
#endif  // OS_ANDROID

class BundledExchangesBrowserTestBase : public ContentBrowserTest {
 protected:
  BundledExchangesBrowserTestBase() = default;
  ~BundledExchangesBrowserTestBase() override = default;

  void NavigateToBundleAndWaitForReady(const GURL& test_data_url,
                                       const GURL& expected_commit_url) {
    base::string16 expected_title = base::ASCIIToUTF16("Ready");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), test_data_url,
                              expected_commit_url));
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

  void NavigateToURLAndWaitForTitle(const GURL& url, const std::string& title) {
    base::string16 title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    EXPECT_TRUE(ExecuteScript(
        shell()->web_contents(),
        base::StringPrintf("location.href = '%s';", url.spec().c_str())));
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesBrowserTestBase);
};

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

ContentBrowserClient* MaybeSetBrowserClientForTesting(
    ContentBrowserClient* browser_client) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/864403): It seems that we call unsupported Android APIs on
  // KitKat when we set a ContentBrowserClient. Don't call such APIs and make
  // this test available on KitKat.
  int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  if (major_version < 5)
    return nullptr;
#endif  // defined(OS_ANDROID)
  return SetBrowserClientForTesting(browser_client);
}

}  // namespace

class InvalidTrustableBundledExchangesFileUrlBrowserTest
    : public ContentBrowserTest {
 protected:
  InvalidTrustableBundledExchangesFileUrlBrowserTest() = default;
  ~InvalidTrustableBundledExchangesFileUrlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = MaybeSetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableBundledExchangesFileUrl,
                                    kInvalidFileUrl);
  }

  ContentBrowserClient* original_client_ = nullptr;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(InvalidTrustableBundledExchangesFileUrlBrowserTest);
};

IN_PROC_BROWSER_TEST_F(InvalidTrustableBundledExchangesFileUrlBrowserTest,
                       NoCrashOnNavigation) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), GURL(kInvalidFileUrl)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_URL, *finish_navigation_observer.error_code());
}

class BundledExchangesTrustableFileBrowserTestBase
    : public BundledExchangesBrowserTestBase {
 protected:
  BundledExchangesTrustableFileBrowserTestBase() = default;
  ~BundledExchangesTrustableFileBrowserTestBase() override = default;

  void SetUp() override { BundledExchangesBrowserTestBase::SetUp(); }

  void SetUpOnMainThread() override {
    BundledExchangesBrowserTestBase::SetUpOnMainThread();
    original_client_ = MaybeSetBrowserClientForTesting(&browser_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableBundledExchangesFileUrl,
                                    test_data_url().spec());
  }

  void TearDownOnMainThread() override {
    BundledExchangesBrowserTestBase::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
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
      test_data_url_ = net::FilePathToFileURL(
          GetTestDataPath("bundled_exchanges_browsertest.wbn"));
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    test_data_url_ = CopyFileAndGetContentUri(
        GetTestDataPath("bundled_exchanges_browsertest.wbn"));
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
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest, RangeRequest) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  RunTestScript("test-range-request.js");
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest, Navigation) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest,
                       NavigationWithHash) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
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
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), test_data_url()));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());
}

class BundledExchangesFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public BundledExchangesBrowserTestBase {
 protected:
  BundledExchangesFileBrowserTest() = default;
  ~BundledExchangesFileBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kBundledHTTPExchanges}, {});
    BundledExchangesBrowserTestBase::SetUp();
  }

  GURL GetTestUrlForFile(base::FilePath file_path) const {
    switch (GetParam()) {
      case TestFilePathMode::kNormalFilePath:
        return net::FilePathToFileURL(file_path);
#if defined(OS_ANDROID)
      case TestFilePathMode::kContentURI:
        return CopyFileAndGetContentUri(file_path);
#endif  // OS_ANDROID
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, BasicNavigation) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, Navigations) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage1Url)));
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage2Url)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, NavigationWithHash) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPageForHashUrl)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest,
                       InvalidBundledExchangeFile) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("invalid_bundled_exchanges.wbn"));

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), test_data_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());
}

INSTANTIATE_TEST_SUITE_P(BundledExchangesFileBrowserTest,
                         BundledExchangesFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

}  // namespace content
