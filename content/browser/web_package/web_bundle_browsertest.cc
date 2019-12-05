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
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

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
      .Append(base::FilePath(FILE_PATH_LITERAL("content/test/data/web_bundle")))
      .AppendASCII(file);
}

#if defined(OS_ANDROID)
GURL CopyFileAndGetContentUri(const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath tmp_dir;
  CHECK(base::GetTempDir(&tmp_dir));
  // The directory name "web_bundle" must be kept in sync with
  // content/shell/android/browsertests_apk/res/xml/file_paths.xml
  base::FilePath tmp_wbn_dir = tmp_dir.AppendASCII("web_bundle");
  CHECK(base::CreateDirectoryAndGetError(tmp_wbn_dir, nullptr));
  base::FilePath tmp_dir_in_tmp_wbn_dir;
  CHECK(
      base::CreateTemporaryDirInDir(tmp_wbn_dir, "", &tmp_dir_in_tmp_wbn_dir));
  base::FilePath temp_file = tmp_dir_in_tmp_wbn_dir.Append(file.BaseName());
  CHECK(base::CopyFile(file, temp_file));
  return GURL(base::GetContentUriFromFilePath(temp_file).value());
}
#endif  // OS_ANDROID

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(DownloadManager* manager) : manager_(manager) {
    manager_->AddObserver(this);
  }
  ~DownloadObserver() override { manager_->RemoveObserver(this); }

  void WaitUntilDownloadCreated() { run_loop_.Run(); }
  const GURL& observed_url() const { return url_; }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    url_ = item->GetURL();
    run_loop_.Quit();
  }

 private:
  DownloadManager* manager_;
  base::RunLoop run_loop_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadObserver);
};

class MockParserFactory;

class MockParser final : public data_decoder::mojom::WebBundleParser {
 public:
  using Index = base::flat_map<GURL, data_decoder::mojom::BundleIndexValuePtr>;

  MockParser(
      MockParserFactory* factory,
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver,
      const Index& index,
      const GURL& primary_url,
      bool simulate_parse_metadata_crash,
      bool simulate_parse_response_crash)
      : factory_(factory),
        receiver_(this, std::move(receiver)),
        index_(index),
        primary_url_(primary_url),
        simulate_parse_metadata_crash_(simulate_parse_metadata_crash),
        simulate_parse_response_crash_(simulate_parse_response_crash) {}
  ~MockParser() override = default;

 private:
  // data_decoder::mojom::WebBundleParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;

  MockParserFactory* factory_;
  mojo::Receiver<data_decoder::mojom::WebBundleParser> receiver_;
  const Index& index_;
  const GURL primary_url_;
  const bool simulate_parse_metadata_crash_;
  const bool simulate_parse_response_crash_;

  DISALLOW_COPY_AND_ASSIGN(MockParser);
};

class MockParserFactory final
    : public data_decoder::mojom::WebBundleParserFactory {
 public:
  MockParserFactory(std::vector<GURL> urls,
                    const base::FilePath& response_body_file)
      : primary_url_(urls[0]) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t response_body_file_size;
    EXPECT_TRUE(
        base::GetFileSize(response_body_file, &response_body_file_size));
    for (const auto& url : urls) {
      data_decoder::mojom::BundleIndexValuePtr item =
          data_decoder::mojom::BundleIndexValue::New();
      item->response_locations.push_back(
          data_decoder::mojom::BundleResponseLocation::New(
              0u, response_body_file_size));
      index_.insert({url, std::move(item)});
    }
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(
            base::BindRepeating(&MockParserFactory::BindWebBundleParserFactory,
                                base::Unretained(this)));
  }
  MockParserFactory(
      const std::vector<std::pair<GURL, const std::string&>> items)
      : primary_url_(items[0].first) {
    uint64_t offset = 0;
    for (const auto& item : items) {
      data_decoder::mojom::BundleIndexValuePtr index_value =
          data_decoder::mojom::BundleIndexValue::New();
      index_value->response_locations.push_back(
          data_decoder::mojom::BundleResponseLocation::New(
              offset, item.second.length()));
      offset += item.second.length();
      index_.insert({item.first, std::move(index_value)});
    }
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(
            base::BindRepeating(&MockParserFactory::BindWebBundleParserFactory,
                                base::Unretained(this)));
  }
  ~MockParserFactory() override = default;

  int GetParserCreationCount() const { return parser_creation_count_; }
  void SimulateParserDisconnect() { parser_ = nullptr; }
  void SimulateParseMetadataCrash() { simulate_parse_metadata_crash_ = true; }
  void SimulateParseResponseCrash() { simulate_parse_response_crash_ = true; }

 private:
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParserFactory>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // data_decoder::mojom::WebBundleParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver,
      base::File file) override {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      file.Close();
    }
    DCHECK(!parser_);
    parser_ = std::make_unique<MockParser>(
        this, std::move(receiver), index_, primary_url_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_);
    parser_creation_count_++;
  }

  void GetParserForDataSource(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver,
      mojo::PendingRemote<data_decoder::mojom::BundleDataSource> data_source)
      override {
    DCHECK(!parser_);
    parser_ = std::make_unique<MockParser>(
        this, std::move(receiver), index_, primary_url_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_);
    parser_creation_count_++;
  }

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  mojo::ReceiverSet<data_decoder::mojom::WebBundleParserFactory> receivers_;
  bool simulate_parse_metadata_crash_ = false;
  bool simulate_parse_response_crash_ = false;
  std::unique_ptr<MockParser> parser_;
  int parser_creation_count_ = 0;
  base::flat_map<GURL, data_decoder::mojom::BundleIndexValuePtr> index_;
  const GURL primary_url_;

  DISALLOW_COPY_AND_ASSIGN(MockParserFactory);
};

void MockParser::ParseMetadata(ParseMetadataCallback callback) {
  if (simulate_parse_metadata_crash_) {
    factory_->SimulateParserDisconnect();
    return;
  }

  base::flat_map<GURL, data_decoder::mojom::BundleIndexValuePtr> items;
  for (const auto& item : index_) {
    items.insert({item.first, item.second.Clone()});
  }

  data_decoder::mojom::BundleMetadataPtr metadata =
      data_decoder::mojom::BundleMetadata::New();
  metadata->primary_url = primary_url_;
  metadata->requests = std::move(items);

  std::move(callback).Run(std::move(metadata), nullptr);
}

void MockParser::ParseResponse(uint64_t response_offset,
                               uint64_t response_length,
                               ParseResponseCallback callback) {
  if (simulate_parse_response_crash_) {
    factory_->SimulateParserDisconnect();
    return;
  }
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->response_headers.insert({"content-type", "text/html"});
  response->payload_offset = response_offset;
  response->payload_length = response_length;
  std::move(callback).Run(std::move(response), nullptr);
}

class WebBundleBrowserTestBase : public ContentBrowserTest {
 protected:
  WebBundleBrowserTestBase() = default;
  ~WebBundleBrowserTestBase() override = default;

  void NavigateAndWaitForTitle(const GURL& test_data_url,
                               const GURL& expected_commit_url,
                               base::StringPiece ascii_title) {
    base::string16 expected_title = base::ASCIIToUTF16(ascii_title);
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), test_data_url,
                              expected_commit_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void NavigateToBundleAndWaitForReady(const GURL& test_data_url,
                                       const GURL& expected_commit_url) {
    NavigateAndWaitForTitle(test_data_url, expected_commit_url, "Ready");
  }

  void RunTestScript(const std::string& script) {
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                              "loadScript('" + script + "');"));
    base::string16 ok = base::ASCIIToUTF16("OK");
    TitleWatcher title_watcher(shell()->web_contents(), ok);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    EXPECT_EQ(ok, title_watcher.WaitAndGetTitle());
  }

  void ExecuteScriptAndWaitForTitle(const std::string& script,
                                    const std::string& title) {
    base::string16 title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

  void NavigateToURLAndWaitForTitle(const GURL& url, const std::string& title) {
    ExecuteScriptAndWaitForTitle(
        base::StringPrintf("location.href = '%s';", url.spec().c_str()), title);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebBundleBrowserTestBase);
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

class InvalidTrustableWebBundleFileUrlBrowserTest : public ContentBrowserTest {
 protected:
  InvalidTrustableWebBundleFileUrlBrowserTest() = default;
  ~InvalidTrustableWebBundleFileUrlBrowserTest() override = default;

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
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    kInvalidFileUrl);
  }

  ContentBrowserClient* original_client_ = nullptr;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(InvalidTrustableWebBundleFileUrlBrowserTest);
};

IN_PROC_BROWSER_TEST_F(InvalidTrustableWebBundleFileUrlBrowserTest,
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

class WebBundleTrustableFileBrowserTestBase : public WebBundleBrowserTestBase {
 protected:
  WebBundleTrustableFileBrowserTestBase() = default;
  ~WebBundleTrustableFileBrowserTestBase() override = default;

  void SetUp() override { WebBundleBrowserTestBase::SetUp(); }

  void SetUpOnMainThread() override {
    WebBundleBrowserTestBase::SetUpOnMainThread();
    original_client_ = MaybeSetBrowserClientForTesting(&browser_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    test_data_url().spec());
  }

  void TearDownOnMainThread() override {
    WebBundleBrowserTestBase::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  const GURL& test_data_url() const { return test_data_url_; }
  const GURL& empty_page_url() const { return empty_page_url_; }

  ContentBrowserClient* original_client_ = nullptr;
  GURL test_data_url_;
  GURL empty_page_url_;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleTrustableFileBrowserTestBase);
};

enum class TestFilePathMode {
  kNormalFilePath,
#if defined(OS_ANDROID)
  kContentURI,
#endif  // OS_ANDROID
};

class WebBundleTrustableFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public WebBundleTrustableFileBrowserTestBase {
 protected:
  WebBundleTrustableFileBrowserTest() {
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      test_data_url_ =
          net::FilePathToFileURL(GetTestDataPath("web_bundle_browsertest.wbn"));
      empty_page_url_ =
          net::FilePathToFileURL(GetTestDataPath("empty_page.html"));
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    test_data_url_ =
        CopyFileAndGetContentUri(GetTestDataPath("web_bundle_browsertest.wbn"));
    empty_page_url_ =
        CopyFileAndGetContentUri(GetTestDataPath("empty_page.html"));
#endif  // OS_ANDROID
  }
  ~WebBundleTrustableFileBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebBundleTrustableFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       TrustableWebBundleFile) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, RangeRequest) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  RunTestScript("test-range-request.js");
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, Navigations) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  // Move to page 1.
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));
  // Move to page 2.
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));
  // Back to page 1.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Back to the initial page.
  ExecuteScriptAndWaitForTitle("history.back();", "Ready");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL(kTestPageUrl));

  // Move to page 1.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Reload.
  ExecuteScriptAndWaitForTitle("document.title = 'reset';", "reset");
  ExecuteScriptAndWaitForTitle("location.reload();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Move to page 2.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));

  // Move out of the Web Bundle.
  NavigateAndWaitForTitle(empty_page_url(), empty_page_url(), "Empty Page");

  // Back to the page 2.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, NavigationWithHash) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
}

INSTANTIATE_TEST_SUITE_P(WebBundleTrustableFileBrowserTests,
                         WebBundleTrustableFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

class WebBundleTrustableFileNotFoundBrowserTest
    : public WebBundleTrustableFileBrowserTestBase {
 protected:
  WebBundleTrustableFileNotFoundBrowserTest() {
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_data_url_ =
        net::FilePathToFileURL(test_data_dir.AppendASCII("not_found"));
  }
  ~WebBundleTrustableFileNotFoundBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebBundleTrustableFileNotFoundBrowserTest, NotFound) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url()));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());
  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ("Failed to read metadata of Web Bundle file: FILE_ERROR_FAILED",
            console_delegate.message());
}

class WebBundleFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public WebBundleBrowserTestBase {
 protected:
  WebBundleFileBrowserTest() = default;
  ~WebBundleFileBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundles}, {});
    WebBundleBrowserTestBase::SetUp();
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

  DISALLOW_COPY_AND_ASSIGN(WebBundleFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, BasicNavigation) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, Navigations) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));
  // Move to page 1.
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));
  // Move to page 2.
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));

  // Back to page 1.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));
  // Back to the initial page.
  ExecuteScriptAndWaitForTitle("history.back();", "Ready");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPageUrl)));

  // Move to page 1.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));

  // Reload.
  ExecuteScriptAndWaitForTitle("document.title = 'reset';", "reset");
  ExecuteScriptAndWaitForTitle("location.reload();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));

  // Move to page 2.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));

  const GURL empty_page_url =
      GetTestUrlForFile(GetTestDataPath("empty_page.html"));

  // Move out of the Web Bundle.
  NavigateAndWaitForTitle(empty_page_url, empty_page_url, "Empty Page");

  // Back to the page 2 in the Web Bundle.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, NavigationWithHash) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPageForHashUrl)));
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, InvalidWebBundleFile) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("invalid_web_bundle.wbn"));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ("Failed to read metadata of Web Bundle file: Wrong magic bytes.",
            console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       ResponseParseErrorInMainResource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_first_entry.wbn"));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       ResponseParseErrorInSubresource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_script_entry.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "load failed");

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, NoLocalFileScheme) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));

  auto expected_title = base::ASCIIToUTF16("load failed");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Local Script"));

  const GURL script_file_url =
      net::FilePathToFileURL(GetTestDataPath("local_script.js"));
  const std::string script = base::StringPrintf(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "%s";
    document.body.appendChild(script);)",
                                                script_file_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, DataDecoderRestart) {
  // The content of this file will be read as response body of any exchange.
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory(
      {GURL(kTestPageUrl), GURL(kTestPage1Url), GURL(kTestPage2Url)},
      test_file_path);
  const GURL test_data_url = GetTestUrlForFile(test_file_path);
  NavigateAndWaitForTitle(test_data_url,
                          web_bundle_utils::GetSynthesizedUrlForWebBundle(
                              test_data_url, GURL(kTestPageUrl)),
                          kTestPageUrl);

  EXPECT_EQ(1, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), kTestPage1Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));

  EXPECT_EQ(2, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), kTestPage2Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));

  EXPECT_EQ(3, mock_factory.GetParserCreationCount());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, ParseMetadataCrash) {
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory({GURL(kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseMetadataCrash();

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, GetTestUrlForFile(test_file_path)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read metadata of Web Bundle file: Cannot connect to the "
      "remote parser service",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, ParseResponseCrash) {
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory({GURL(kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseResponseCrash();

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, GetTestUrlForFile(test_file_path)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Cannot connect to "
      "the remote parser service",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, Variants) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("variants_test.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'application/octet-stream'};
      const resp = await fetch('/data', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "octet-stream");
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'application/json'};
      const resp = await fetch('/data', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "json");
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'foo/bar'};
      const resp = await fetch('/data', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "octet-stream");
  // TODO(crbug/1029406): Test Accept-Language negotiation.
}

INSTANTIATE_TEST_SUITE_P(WebBundleFileBrowserTest,
                         WebBundleFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

class WebBundleNetworkBrowserTest : public WebBundleBrowserTestBase {
 protected:
  // Keep consistent with NETWORK_TEST_PORT in generate-test-wbns.sh.
  static constexpr int kNetworkTestPort = 39600;

  WebBundleNetworkBrowserTest() = default;
  ~WebBundleNetworkBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebBundleBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundlesFromNetwork}, {});
    WebBundleBrowserTestBase::SetUp();
  }

  void RegisterRequestHandler(const std::string& relative_url,
                              const std::string& headers,
                              const std::string& contents) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const std::string& relative_url, const std::string& headers,
           const std::string& contents,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != relative_url)
            return nullptr;
          return std::make_unique<net::test_server::RawHttpResponse>(headers,
                                                                     contents);
        },
        relative_url, headers, contents));
  }

  std::string GetTestFile(const std::string& file_name) const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string contents;
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath test_path =
        src_dir.Append(FILE_PATH_LITERAL("content/test/data/web_bundle"));
    CHECK(base::ReadFileToString(test_path.AppendASCII(file_name), &contents));
    return contents;
  }

  void TestNavigationFailure(const GURL& url,
                             const std::string& expected_console_error) {
    WebContents* web_contents = shell()->web_contents();
    ConsoleObserverDelegate console_delegate(web_contents, "*");
    web_contents->SetDelegate(&console_delegate);
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(web_contents,
                                                        run_loop.QuitClosure());
    EXPECT_FALSE(NavigateToURL(web_contents, url));
    run_loop.Run();
    ASSERT_TRUE(finish_navigation_observer.error_code());
    EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
              *finish_navigation_observer.error_code());
    if (console_delegate.messages().empty())
      console_delegate.Wait();
    EXPECT_FALSE(console_delegate.messages().empty());
    EXPECT_EQ(expected_console_error, console_delegate.message());
  }

  void RunHistoryNavigationErrorTest(
      const std::string& first_headers,
      const std::string& first_contents,
      const std::string& subsequent_headers,
      const std::string& subsequent_contents,
      const std::string& expected_error_message) {
    scoped_refptr<base::RefCountedData<bool>> is_first_call =
        base::MakeRefCounted<base::RefCountedData<bool>>(true);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const std::string& relative_url,
           scoped_refptr<base::RefCountedData<bool>> is_first_call,
           const std::string& first_headers, const std::string& first_contents,
           const std::string& subsequent_headers,
           const std::string& subsequent_contents,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != relative_url)
            return nullptr;
          if (is_first_call->data) {
            is_first_call->data = false;
            return std::make_unique<net::test_server::RawHttpResponse>(
                first_headers, first_contents);
          } else {
            return std::make_unique<net::test_server::RawHttpResponse>(
                subsequent_headers, subsequent_contents);
          }
        },
        "/web_bundle/path_test/in_scope/path_test.wbn",
        std::move(is_first_call), first_headers, first_contents,
        subsequent_headers, subsequent_contents));
    ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
    NavigateToBundleAndWaitForReady(
        GURL(base::StringPrintf(
            "http://localhost:%d/web_bundle/path_test/in_scope/path_test.wbn",
            kNetworkTestPort)),
        GURL(base::StringPrintf(
            "http://localhost:%d/web_bundle/path_test/in_scope/",
            kNetworkTestPort)));
    NavigateToURLAndWaitForTitle(
        GURL(base::StringPrintf(
            "http://localhost:%d/web_bundle/path_test/out_scope/page.html",
            kNetworkTestPort)),
        "Out scope page from server / out scope script from server");

    WebContents* web_contents = shell()->web_contents();
    ConsoleObserverDelegate console_delegate(web_contents, "*");
    web_contents->SetDelegate(&console_delegate);

    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(web_contents,
                                                        run_loop.QuitClosure());
    EXPECT_TRUE(ExecuteScript(web_contents, "history.back();"));

    run_loop.Run();
    ASSERT_TRUE(finish_navigation_observer.error_code());
    EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
              *finish_navigation_observer.error_code());

    if (console_delegate.messages().empty())
      console_delegate.Wait();

    EXPECT_FALSE(console_delegate.messages().empty());
    EXPECT_EQ(expected_error_message, console_delegate.message());
  }

  static GURL GetTestUrl(const std::string& host) {
    return GURL(base::StringPrintf("http://%s:%d/web_bundle/test.wbn",
                                   host.c_str(), kNetworkTestPort));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleNetworkBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Simple) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  NavigateToBundleAndWaitForReady(
      GetTestUrl("localhost"),
      GURL(base::StringPrintf("http://localhost:%d/web_bundle/network/",
                              kNetworkTestPort)));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Download) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  // Web Bundle file with attachment Content-Disposition must trigger download.
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Disposition:attachment; filename=test.wbn\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  const GURL url = GetTestUrl("localhost");
  WebContents* web_contents = shell()->web_contents();
  std::unique_ptr<DownloadObserver> download_observer =
      std::make_unique<DownloadObserver>(BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext()));
  EXPECT_FALSE(NavigateToURL(web_contents, url));
  download_observer->WaitUntilDownloadCreated();
  EXPECT_EQ(url, download_observer->observed_url());
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, NoContentLength) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RegisterRequestHandler("/web_bundle/test.wbn",
                         "HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n",
                         test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "Web Bundle response must have valid Content-Length header.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, NonSecureUrl) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("example.com"),
      "Web Bundle response must be served from HTTPS or localhost HTTP.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, PrimaryURLNotFound) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network_primary_url_not_found.wbn");

  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "The primary URL resource is not found in the web bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, OriginMismatch) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("127.0.0.1"),
      "The origin of primary URL doesn't match with the origin of the web "
      "bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, InvalidFile) {
  const std::string test_bundle = GetTestFile("invalid_web_bundle.wbn");
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "Failed to read metadata of Web Bundle file: Wrong magic bytes.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, DataDecoderRestart) {
  const GURL primary_url(base::StringPrintf(
      "http://localhost:%d/web_bundle/network/", kNetworkTestPort));
  const GURL script_url(base::StringPrintf(
      "http://localhost:%d/web_bundle/network/script.js", kNetworkTestPort));
  const std::string primary_url_content = "<title>Ready</title>";
  const std::string script_url_content = "document.title = 'OK'";
  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, primary_url_content}, {script_url, script_url_content}};
  const std::string test_bundle = primary_url_content + script_url_content;
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);

  MockParserFactory mock_factory(items);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));

  const GURL test_data_url = GetTestUrl("localhost");
  NavigateToBundleAndWaitForReady(
      test_data_url,
      GURL(base::StringPrintf("http://localhost:%d/web_bundle/network/",
                              kNetworkTestPort)));

  EXPECT_EQ(1, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "OK");

  EXPECT_EQ(2, mock_factory.GetParserCreationCount());
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ParseMetadataCrash) {
  const GURL primary_url(base::StringPrintf(
      "http://localhost:%d/web_bundle/network/", kNetworkTestPort));
  const std::string test_bundle = "<title>Ready</title>";
  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, test_bundle}};
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  MockParserFactory mock_factory(items);
  mock_factory.SimulateParseMetadataCrash();
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));

  const GURL test_data_url = GetTestUrl("localhost");
  TestNavigationFailure(test_data_url,
                        "Failed to read metadata of Web Bundle file: Cannot "
                        "connect to the remote parser service");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ParseResponseCrash) {
  const GURL primary_url(base::StringPrintf(
      "http://localhost:%d/web_bundle/network/", kNetworkTestPort));
  const std::string test_bundle = "<title>Ready</title>";
  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, test_bundle}};
  RegisterRequestHandler(
      "/web_bundle/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  MockParserFactory mock_factory(items);
  mock_factory.SimulateParseResponseCrash();
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));

  const GURL test_data_url = GetTestUrl("localhost");
  TestNavigationFailure(test_data_url,
                        "Failed to read response header of Web Bundle file: "
                        "Cannot connect to the remote parser service");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, PathMismatch) {
  const std::string test_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RegisterRequestHandler(
      "/other_dir/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GURL(base::StringPrintf("http://localhost:%d/other_dir/"
                              "test.wbn",
                              kNetworkTestPort)),
      base::StringPrintf(
          "Path restriction mismatch: Can't navigate to "
          "http://localhost:%d/web_bundle/network/ in the web bundle served "
          "from http://localhost:%d/other_dir/test.wbn.",
          kNetworkTestPort, kNetworkTestPort));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Navigations) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  RegisterRequestHandler(
      "/web_bundle/path_test/in_scope/path_test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));

  NavigateToBundleAndWaitForReady(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/path_test.wbn",
          kNetworkTestPort)),
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/",
          kNetworkTestPort)));

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/page.html",
          kNetworkTestPort)),
      "In scope page in Web Bundle / in scope script in Web Bundle");

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/out_scope/page.html",
          kNetworkTestPort)),
      "Out scope page from server / out scope script from server");

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/page.html",
          kNetworkTestPort)),
      "In scope page from server / in scope script from server");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, HistoryNavigations) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  RegisterRequestHandler(
      "/web_bundle/path_test/in_scope/path_test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));

  NavigateToBundleAndWaitForReady(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/path_test.wbn",
          kNetworkTestPort)),
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/",
          kNetworkTestPort)));

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/page.html",
          kNetworkTestPort)),
      "In scope page in Web Bundle / in scope script in Web Bundle");

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/in_scope/",
          kNetworkTestPort)),
      "Ready");

  ExecuteScriptAndWaitForTitle(
      "history.back();",
      "In scope page in Web Bundle / in scope script in Web Bundle");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(base::StringPrintf(
                "http://localhost:%d/web_bundle/path_test/in_scope/page.html",
                kNetworkTestPort)));

  NavigateToURLAndWaitForTitle(
      GURL(base::StringPrintf(
          "http://localhost:%d/web_bundle/path_test/out_scope/page.html",
          kNetworkTestPort)),
      "Out scope page from server / out scope script from server");

  ExecuteScriptAndWaitForTitle(
      "history.back();",
      "In scope page in Web Bundle / in scope script in Web Bundle");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(base::StringPrintf(
                "http://localhost:%d/web_bundle/path_test/in_scope/page.html",
                kNetworkTestPort)));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_UnexpectedContentType) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  RunHistoryNavigationErrorTest(
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle,
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/foo_bar\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle, "Unexpected content type.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_UnexpectedRedirect) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  RunHistoryNavigationErrorTest(
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle,
      "HTTP/1.1 302 OK\n"
      "Location:/web_bundle/empty_page.html\n",
      "", "Unexpected redirect.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_InvalidContentLength) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  RunHistoryNavigationErrorTest(
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle,
      "HTTP/1.1 200 OK\n"
      "Content-Type:application/webbundle\n"
      "Cache-Control:no-store\n",
      test_bundle,
      "Web Bundle response must have valid Content-Length header.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_ReadMetadataFailure) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  const std::string invalid_bundle = GetTestFile("invalid_web_bundle.wbn");
  RunHistoryNavigationErrorTest(
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle,
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         invalid_bundle.size()),
      invalid_bundle,
      "Failed to read metadata of Web Bundle file: Wrong magic bytes.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_ExpectedUrlNotFound) {
  const std::string test_bundle = GetTestFile("path_test.wbn");
  const std::string other_bundle =
      GetTestFile("web_bundle_browsertest_network.wbn");
  RunHistoryNavigationErrorTest(
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle,
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Cache-Control:no-store\n"
                         "Content-Length: %" PRIuS "\n",
                         other_bundle.size()),
      other_bundle,
      "The expected URL resource is not found in the web bundle.");
}

}  // namespace content
