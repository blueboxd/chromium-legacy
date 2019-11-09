// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

class OneShotNavigationObserver : public NavigationObserver {
 public:
  explicit OneShotNavigationObserver(Shell* shell) : tab_(shell->tab()) {
    tab_->GetNavigationController()->AddObserver(this);
  }

  ~OneShotNavigationObserver() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

  void WaitForNavigation() { run_loop_.Run(); }

  bool completed() { return completed_; }
  bool is_error_page() { return is_error_page_; }
  Navigation::LoadError load_error() { return load_error_; }

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override {
    completed_ = true;
    Finish(navigation);
  }

  void NavigationFailed(Navigation* navigation) override { Finish(navigation); }

  void Finish(Navigation* navigation) {
    is_error_page_ = navigation->IsErrorPage();
    load_error_ = navigation->GetLoadError();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  Tab* tab_;
  bool completed_ = false;
  bool is_error_page_ = false;
  Navigation::LoadError load_error_ = Navigation::kNoError;
};

}  // namespace

using NavigationBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NoError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kNoError);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpClientError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/non_existent.html"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpClientError);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpServerError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/echo?status=500"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpServerError);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SSLError) {
  net::EmbeddedTestServer https_server_mismatched(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_mismatched.SetSSLConfig(
      net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server_mismatched.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  ASSERT_TRUE(https_server_mismatched.Start());

  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(
      https_server_mismatched.GetURL("/simple_page.html"));

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kSSLError);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpConnectivityError) {
  GURL url("http://doesntexist.com/foo");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      url, net::ERR_NAME_NOT_RESOLVED);

  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(url);

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kConnectivityError);
}

}  // namespace weblayer
