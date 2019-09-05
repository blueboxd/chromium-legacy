// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"

namespace content {
namespace {

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool CanAcceptUntrustedExchangesIfNeeded() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserClient);
};

}  // namespace

class BundledExchangesTrustableFileBrowserTest : public ContentBrowserTest {
 protected:
  BundledExchangesTrustableFileBrowserTest()
      : test_data_path_(GetTestDataPath()) {}
  ~BundledExchangesTrustableFileBrowserTest() override = default;

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
    command_line->AppendSwitchPath(switches::kTrustableBundledExchangesFile,
                                   test_data_path());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  const base::FilePath& test_data_path() const { return test_data_path_; }

  ContentBrowserClient* original_client_ = nullptr;

 private:
  base::FilePath GetTestDataPath() {
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    return test_data_dir.AppendASCII("services")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("bundled_exchanges")
        .AppendASCII("hello.wbn");
  }

  TestBrowserClient browser_client_;
  const base::FilePath test_data_path_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesTrustableFileBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BundledExchangesTrustableFileBrowserTest,
                       TrustableBundledExchangesFile) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  base::string16 expected_title = base::ASCIIToUTF16("Done");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(),
                            net::FilePathToFileURL(test_data_path()),
                            GURL("https://test.example.org/")));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

}  // namespace content
