// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>

#include "base/bind.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

class WebRunnerSmokeTest : public testing::Test {
 public:
  WebRunnerSmokeTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {}
  void SetUp() final {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebRunnerSmokeTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() == "/test.html") {
      EXPECT_FALSE(test_html_requested_);
      test_html_requested_ = true;
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("<!doctype html><img src=\"/img.png\">");
      http_response->set_content_type("text/html");
      return http_response;
    } else if (absolute_url.path() == "/window_close.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(
          "<!doctype html><script>window.close();</script>");
      http_response->set_content_type("text/html");
      return http_response;
    } else if (absolute_url.path() == "/img.png") {
      EXPECT_FALSE(test_image_requested_);
      test_image_requested_ = true;
      // All done!
      run_loop_.Quit();
    }
    return nullptr;
  }

 protected:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  bool test_html_requested_ = false;
  bool test_image_requested_ = false;

  base::MessageLoopForIO message_loop_;

  net::EmbeddedTestServer test_server_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerSmokeTest);
};

// Verify that the Component loads and fetches the desired page.
TEST_F(WebRunnerSmokeTest, RequestHtmlAndImage) {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = test_server_.GetURL("/test.html").spec();

  auto launcher = base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
                      ->ConnectToServiceSync<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerSyncPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

  run_loop_.Run();

  EXPECT_TRUE(test_html_requested_);
  EXPECT_TRUE(test_image_requested_);
}

// Verify that the Component can be terminated via the Lifecycle API.
TEST_F(WebRunnerSmokeTest, LifecycleTerminate) {
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = test_server_.GetURL("/test.html").spec();
  launch_info.directory_request = directory.NewRequest().TakeChannel();

  auto launcher = base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
                      ->ConnectToServiceSync<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

  base::fuchsia::ServiceDirectoryClient component_services(
      std::move(directory));
  auto lifecycle =
      component_services.ConnectToService<fuchsia::modular::Lifecycle>();
  ASSERT_TRUE(lifecycle);

  // Terminate() the component, and expect that |controller| disconnects us.
  base::RunLoop loop;
  controller.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  lifecycle->Terminate();
  loop.Run();

  EXPECT_FALSE(controller);
}

// Verify that if the Frame disconnects, the Component tears down.
TEST_F(WebRunnerSmokeTest, ComponentExitOnFrameClose) {
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = test_server_.GetURL("/window_close.html").spec();
  launch_info.directory_request = directory.NewRequest().TakeChannel();

  auto launcher = base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
                      ->ConnectToServiceSync<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

  // Script in the page will execute window.close(), which should teardown the
  // Component, causing |controller| to be disconnected.
  base::RunLoop loop;
  controller.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  loop.Run();

  EXPECT_FALSE(controller);
}

}  // anonymous namespace
