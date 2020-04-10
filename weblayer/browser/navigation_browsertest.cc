// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
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

class StopNavigationObserver : public NavigationObserver {
 public:
  StopNavigationObserver(NavigationController* controller, bool stop_in_start)
      : controller_(controller), stop_in_start_(stop_in_start) {
    controller_->AddObserver(this);
  }
  ~StopNavigationObserver() override { controller_->RemoveObserver(this); }

  void WaitForNavigation() { run_loop_.Run(); }

  // NavigationObserver:
  void NavigationStarted(Navigation* navigation) override {
    if (stop_in_start_)
      controller_->Stop();
  }
  void NavigationRedirected(Navigation* navigation) override {
    if (!stop_in_start_)
      controller_->Stop();
  }
  void NavigationFailed(Navigation* navigation) override { run_loop_.Quit(); }

 private:
  NavigationController* controller_;
  // If true Stop() is called in NavigationStarted(), otherwise Stop() is
  // called in NavigationRedirected().
  const bool stop_in_start_;
  base::RunLoop run_loop_;
};

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
  int http_status_code() { return http_status_code_; }
  NavigationState navigation_state() { return navigation_state_; }

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
    http_status_code_ = navigation->GetHttpStatusCode();
    navigation_state_ = navigation->GetState();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  Tab* tab_;
  bool completed_ = false;
  bool is_error_page_ = false;
  Navigation::LoadError load_error_ = Navigation::kNoError;
  int http_status_code_ = 0;
  NavigationState navigation_state_ = NavigationState::kWaitingResponse;
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
  EXPECT_EQ(observer.http_status_code(), 200);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
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
  EXPECT_EQ(observer.http_status_code(), 404);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
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
  EXPECT_EQ(observer.http_status_code(), 500);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
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
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
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
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnStart) {
  EXPECT_TRUE(embedded_test_server()->Start());
  StopNavigationObserver observer(shell()->tab()->GetNavigationController(),
                                  true);
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));

  observer.WaitForNavigation();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnRedirect) {
  EXPECT_TRUE(embedded_test_server()->Start());
  StopNavigationObserver observer(shell()->tab()->GetNavigationController(),
                                  false);
  const GURL original_url = embedded_test_server()->GetURL("/simple_page.html");
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/server-redirect?" +
                                     original_url.spec()));

  observer.WaitForNavigation();
}

namespace {

class HeaderInjectorNavigationObserver : public NavigationObserver {
 public:
  HeaderInjectorNavigationObserver(Shell* shell,
                                   const std::string& header_name,
                                   const std::string& header_value,
                                   bool inject_in_start)
      : tab_(shell->tab()),
        header_name_(header_name),
        header_value_(header_value),
        inject_in_start_(inject_in_start) {
    tab_->GetNavigationController()->AddObserver(this);
  }

  ~HeaderInjectorNavigationObserver() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

 private:
  // NavigationObserver implementation:
  void NavigationStarted(Navigation* navigation) override {
    if (inject_in_start_)
      InjectHeaders(navigation);
  }

  void NavigationRedirected(Navigation* navigation) override {
    if (!inject_in_start_)
      InjectHeaders(navigation);
  }

  void InjectHeaders(Navigation* navigation) {
    navigation->SetRequestHeader(header_name_, header_value_);
  }

  Tab* tab_;
  const std::string header_name_;
  const std::string header_value_;
  // If true, header is set in start, otherwise header is set in redirect.
  const bool inject_in_start_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeader) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "header";
  const std::string header_value = "value";
  HeaderInjectorNavigationObserver observer(shell(), header_name, header_value,
                                            true);

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // Header should be present in initial request.
  EXPECT_EQ(header_value, response_1.http_request()->headers.at(header_name));
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  // Header should carry through to redirect.
  response_2.WaitForRequest();
  EXPECT_EQ(header_value, response_2.http_request()->headers.at(header_name));
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeaderInRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "header";
  const std::string header_value = "value";
  HeaderInjectorNavigationObserver observer(shell(), header_name, header_value,
                                            false);
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // Header should not be present in initial request.
  EXPECT_FALSE(base::Contains(response_1.http_request()->headers, header_name));

  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  response_2.WaitForRequest();

  // Header should be in redirect.
  EXPECT_EQ(header_value, response_2.http_request()->headers.at(header_name));
}

}  // namespace weblayer
