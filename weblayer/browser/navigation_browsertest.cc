// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
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

// NavigationObserver that allows registering a callback for various
// NavigationObserver functions.
class NavigationObserverImpl : public NavigationObserver {
 public:
  explicit NavigationObserverImpl(NavigationController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }
  ~NavigationObserverImpl() override { controller_->RemoveObserver(this); }

  using Callback = base::RepeatingCallback<void(Navigation*)>;

  void SetStartedCallback(Callback callback) {
    started_callback_ = std::move(callback);
  }

  void SetRedirectedCallback(Callback callback) {
    redirected_callback_ = std::move(callback);
  }

  void SetFailedClosure(base::RepeatingClosure closure) {
    failed_closure_ = std::move(closure);
  }

  void SetCompletedClosure(Callback closure) {
    completed_closure_ = std::move(closure);
  }

  // NavigationObserver:
  void NavigationStarted(Navigation* navigation) override {
    if (started_callback_)
      started_callback_.Run(navigation);
  }
  void NavigationRedirected(Navigation* navigation) override {
    if (redirected_callback_)
      redirected_callback_.Run(navigation);
  }
  void NavigationCompleted(Navigation* navigation) override {
    if (completed_closure_)
      completed_closure_.Run(navigation);
  }
  void NavigationFailed(Navigation* navigation) override {
    if (failed_closure_)
      failed_closure_.Run();
  }

 private:
  NavigationController* controller_;
  Callback started_callback_;
  Callback redirected_callback_;
  Callback completed_closure_;
  base::RepeatingClosure failed_closure_;
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

class NavigationBrowserTest : public WebLayerBrowserTest {
 public:
  NavigationController* GetNavigationController() {
    return shell()->tab()->GetNavigationController();
  }
};

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NoError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
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
  GetNavigationController()->Navigate(
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
  GetNavigationController()->Navigate(
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
  GetNavigationController()->Navigate(
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
  GetNavigationController()->Navigate(url);

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kConnectivityError);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnStart) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(base::BindLambdaForTesting(
      [&](Navigation*) { GetNavigationController()->Stop(); }));
  observer.SetFailedClosure(
      base::BindRepeating(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetRedirectedCallback(base::BindLambdaForTesting(
      [&](Navigation*) { GetNavigationController()->Stop(); }));
  observer.SetFailedClosure(
      base::BindRepeating(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  const GURL original_url = embedded_test_server()->GetURL("/simple_page.html");
  GetNavigationController()->Navigate(embedded_test_server()->GetURL(
      "/server-redirect?" + original_url.spec()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       NavigateFromRendererInitiatedNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationController* controller = shell()->tab()->GetNavigationController();
  const GURL final_url = embedded_test_server()->GetURL("/simple_page2.html");
  int failed_count = 0;
  int completed_count = 0;
  NavigationObserverImpl observer(controller);
  base::RunLoop run_loop;
  observer.SetFailedClosure(
      base::BindLambdaForTesting([&]() { failed_count++; }));
  observer.SetCompletedClosure(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        completed_count++;
        if (navigation->GetURL().path() == "/simple_page2.html")
          run_loop.Quit();
      }));
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        if (navigation->GetURL().path() == "/simple_page.html")
          controller->Navigate(final_url);
      }));
  controller->Navigate(embedded_test_server()->GetURL("/simple_page4.html"));
  run_loop.Run();
  EXPECT_EQ(1, failed_count);
  EXPECT_EQ(2, completed_count);
  ASSERT_EQ(2, controller->GetNavigationListSize());
  EXPECT_EQ(final_url, controller->GetNavigationEntryDisplayURL(1));
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeader) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "header";
  const std::string header_value = "value";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));

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
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetRedirectedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));
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
