// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr gfx::Size kSize(640, 480);

}  // namespace

class IsolatedPrerenderBrowserTest
    : public InProcessBrowserTest,
      public prerender::PrerenderHandle::Observer {
 public:
  IsolatedPrerenderBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatePrerenders);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    origin_server_->RegisterRequestMonitor(base::BindRepeating(
        &IsolatedPrerenderBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    ASSERT_TRUE(origin_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    InProcessBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
  }

  std::unique_ptr<prerender::PrerenderHandle> StartPrerender(const GURL& url) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(
            browser()->profile());

    return prerender_manager->AddPrerenderFromNavigationPredictor(
        url,
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .GetDefaultSessionStorageNamespace(),
        kSize);
  }

  GURL GetOriginServerURL(const std::string& path) const {
    return origin_server_->GetURL("testorigin.com", path);
  }

  size_t origin_server_request_with_cookies() const {
    return origin_server_request_with_cookies_;
  }

 protected:
  base::OnceClosure waiting_for_resource_request_closure_;

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &IsolatedPrerenderBrowserTest::MonitorResourceRequestOnUIThread,
            base::Unretained(this), request));
  }

  void MonitorResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Don't care about favicons.
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return;

    if (request.headers.find("Cookie") != request.headers.end()) {
      origin_server_request_with_cookies_++;
    }
  }

  // prerender::PrerenderHandle::Observer:
  void OnPrerenderStart(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStopLoading(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderDomContentLoaded(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderNetworkBytesChanged(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override {
    if (waiting_for_resource_request_closure_) {
      std::move(waiting_for_resource_request_closure_).Run();
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  size_t origin_server_request_with_cookies_ = 0;
};

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest, PrerenderIsIsolated) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GetOriginServerURL("/"),
                                 "testing"));

  // Do a prerender to the same origin and expect that the cookies are not used.
  std::unique_ptr<prerender::PrerenderHandle> handle =
      StartPrerender(GetOriginServerURL("/simple.html"));
  ASSERT_TRUE(handle);

  // Wait for the prerender to complete before checking.
  if (!handle->IsFinishedLoading()) {
    handle->SetObserver(this);
    base::RunLoop run_loop;
    waiting_for_resource_request_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, origin_server_request_with_cookies());

  histogram_tester.ExpectUniqueSample(
      "Prerender.FinalStatus",
      prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED, 1);

  // Navigate to the same origin and expect it to have cookies.
  // Note: This check needs to come after the prerender, otherwise the prerender
  // will be cancelled because the origin was recently loaded.
  ui_test_utils::NavigateToURL(browser(), GetOriginServerURL("/simple.html"));
  EXPECT_EQ(1U, origin_server_request_with_cookies());
}
