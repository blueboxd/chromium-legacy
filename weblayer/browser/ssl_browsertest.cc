// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class SSLBrowserTest : public WebLayerBrowserTest {
 public:
  SSLBrowserTest() = default;
  ~SSLBrowserTest() override = default;

  // WebLayerBrowserTest:
  void PreRunTestOnMainThread() override {
    WebLayerBrowserTest::PreRunTestOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

    https_server_mismatched_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_mismatched_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(https_server_mismatched_->Start());
  }

  void PostRunTestOnMainThread() override {
    https_server_.reset();
    https_server_mismatched_.reset();
    WebLayerBrowserTest::PostRunTestOnMainThread();
  }

  void NavigateToOkPage() {
    ASSERT_EQ("127.0.0.1", ok_url().host());
    NavigateAndWaitForCompletion(ok_url(), shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
  }

  void NavigateToPageWithSslError() {
    // Now do a navigation that should result in an SSL error.
    GURL url_with_mismatched_cert =
        https_server_mismatched_->GetURL("/simple_page.html");

    NavigateAndWaitForFailure(url_with_mismatched_cert, shell());

    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact an SSL interstitial.
    EXPECT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  GURL ok_url() { return https_server_->GetURL("/simple_page.html"); }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_mismatched_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLBrowserTest);
};

// Tests clicking "take me back" on the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TakeMeBack) {
  NavigateToOkPage();
  NavigateToPageWithSslError();

  // Click "Take me back".
  TestNavigationObserver navigation_observer(
      ok_url(), TestNavigationObserver::NavigationEvent::Completion, shell());
  ExecuteScript(shell(), "window.certificateErrorPageController.dontProceed();",
                false /*use_separate_isolate*/);
  navigation_observer.Wait();
  EXPECT_FALSE(IsShowingSSLInterstitial(shell()->tab()));

  // Check that it's possible to navigate to a new page.
  NavigateAndWaitForCompletion(https_server_->GetURL("/simple_page2.html"),
                               shell());
  EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
}

// Tests navigating away from the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, NavigateAway) {
  NavigateToOkPage();
  NavigateToPageWithSslError();

  NavigateAndWaitForCompletion(https_server_->GetURL("/simple_page2.html"),
                               shell());
  EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
}

}  // namespace weblayer
