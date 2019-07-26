// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

class InterceptAndCancelDidCommitProvisionalLoad
    : public DidCommitNavigationInterceptor {
 public:
  explicit InterceptAndCancelDidCommitProvisionalLoad(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}
  ~InterceptAndCancelDidCommitProvisionalLoad() override {}

  void Wait(size_t number_of_messages) {
    while (intercepted_messages_.size() < number_of_messages) {
      loop_.reset(new base::RunLoop);
      loop_->Run();
    }
  }

  const std::vector<NavigationRequest*>& intercepted_navigations() const {
    return intercepted_navigations_;
  }

  const std::vector<::FrameHostMsg_DidCommitProvisionalLoad_Params>&
  intercepted_messages() const {
    return intercepted_messages_;
  }

  std::vector<::service_manager::mojom::InterfaceProviderRequest>&
  intercepted_requests() {
    return intercepted_requests_;
  }

  std::vector<blink::mojom::DocumentInterfaceBrokerRequest>&
  intercepted_broker_content_requests() {
    return intercepted_broker_content_requests_;
  }

  std::vector<blink::mojom::DocumentInterfaceBrokerRequest>&
  intercepted_broker_blink_requests() {
    return intercepted_broker_blink_requests_;
  }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    intercepted_navigations_.push_back(navigation_request);
    intercepted_messages_.push_back(*params);
    intercepted_requests_.push_back(
        *interface_params
            ? std::move((*interface_params)->interface_provider_request)
            : nullptr);
    intercepted_broker_content_requests_.push_back(
        *interface_params
            ? std::move((*interface_params)
                            ->document_interface_broker_content_request)
            : nullptr);
    intercepted_broker_blink_requests_.push_back(
        *interface_params
            ? std::move(
                  (*interface_params)->document_interface_broker_blink_request)
            : nullptr);
    if (loop_)
      loop_->Quit();
    // Do not send the message to the RenderFrameHostImpl.
    return false;
  }

  // Note: Do not dereference the intercepted_navigations_, they are used as
  // indices in the RenderFrameHostImpl and not for themselves.
  std::vector<NavigationRequest*> intercepted_navigations_;
  std::vector<::FrameHostMsg_DidCommitProvisionalLoad_Params>
      intercepted_messages_;
  std::vector<::service_manager::mojom::InterfaceProviderRequest>
      intercepted_requests_;
  std::vector<blink::mojom::DocumentInterfaceBrokerRequest>
      intercepted_broker_content_requests_;
  std::vector<blink::mojom::DocumentInterfaceBrokerRequest>
      intercepted_broker_blink_requests_;
  std::unique_ptr<base::RunLoop> loop_;
};

// Record every WebContentsObserver's event related to navigation. The goal is
// to check these events happen and happen in the expected right order.
class NavigationRecorder : public WebContentsObserver {
 public:
  explicit NavigationRecorder(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver implementation.
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("start " + navigation_handle->GetURL().path());
    WakeUp();
  }
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("ready-to-commit " + navigation_handle->GetURL().path());
    WakeUp();
  }
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("did-commit " + navigation_handle->GetURL().path());
    WakeUp();
  }

  void WaitForEvents(size_t numbers_of_events) {
    while (records_.size() < numbers_of_events) {
      loop_.reset(new base::RunLoop);
      loop_->Run();
      loop_.reset();
    }
  }

  const std::vector<std::string> records() { return records_; }

 private:
  void WakeUp() {
    if (loop_)
      loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> loop_;
  std::vector<std::string> records_;
};

// Used to wait for an observed IPC to be received.
class BrowserMessageObserver : public content::BrowserMessageFilter {
 public:
  BrowserMessageObserver(uint32_t observed_message_class,
                         uint32_t observed_message_type)
      : content::BrowserMessageFilter(observed_message_class),
        observed_message_type_(observed_message_type) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == observed_message_type_)
      loop.Quit();
    return false;
  }

  void Wait() { loop.Run(); }

 private:
  ~BrowserMessageObserver() override {}
  uint32_t observed_message_type_;
  base::RunLoop loop;
  DISALLOW_COPY_AND_ASSIGN(BrowserMessageObserver);
};

}  // namespace

// Test about navigation.
// If you don't need a custom embedded test server, please use the next class
// below (NavigationBrowserTest), it will automatically start the
// default server.
class NavigationBaseBrowserTest : public ContentBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    ToggleNavigationImmediateResponse();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  void ToggleNavigationImmediateResponse() {
    if (GetParam()) {
      feature_list.InitAndDisableFeature(
          features::kNavigationImmediateResponseBody);
    } else {
      feature_list.InitAndEnableFeature(
          features::kNavigationImmediateResponseBody);
    }
  }

  base::test::ScopedFeatureList feature_list;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NavigationBaseBrowserTest,
                         ::testing::Bool());

class NavigationBrowserTest : public NavigationBaseBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationBaseBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

class NetworkIsolationNavigationBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          net::features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list_.InitAndDisableFeature(
          net::features::kAppendFrameOriginToNetworkIsolationKey);
    }
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  // Navigate to |url| and for each ResourceRequest record its
  // trusted_network_isolation_key. Stop listening after |final_resource| has
  // been detected. The output is recorded in |network_isolation_keys|.
  void NavigateAndRecordNetworkIsolationKeys(
      const GURL& url,
      const GURL& final_resource,
      bool from_renderer,
      std::map<GURL, net::NetworkIsolationKey>* network_isolation_keys,
      std::map<GURL, network::mojom::UpdateNetworkIsolationKeyOnRedirect>*
          update_network_isolation_key_on_redirects) {
    if (from_renderer)
      EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

    base::RunLoop run_loop;
    base::Lock lock;

    // Intercept network requests and record them.
    URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](URLLoaderInterceptor::RequestParams* params) -> bool {
          base::AutoLock top_frame_origins_lock(lock);
          (*network_isolation_keys)[params->url_request.url] =
              params->url_request.trusted_network_isolation_key;
          (*update_network_isolation_key_on_redirects)[params->url_request
                                                           .url] =
              params->url_request.update_network_isolation_key_on_redirect;

          if (params->url_request.url == final_resource)
            run_loop.Quit();
          return false;
        }));

    if (from_renderer)
      EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));
    else
      EXPECT_TRUE(NavigateToURL(shell(), url));

    // Wait until the last resource we care about has been requested.
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NetworkIsolationNavigationBrowserTest,
                         ::testing::Bool());

// Ensure that browser initiated basic navigations work.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, BrowserInitiatedNavigations) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_FALSE(observer.last_initiator_origin().has_value());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Perform a same site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_FALSE(observer.last_initiator_origin().has_value());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());

  // Perform a cross-site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url = embedded_test_server()->GetURL("foo.com", "/title3.html");
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_FALSE(observer.last_initiator_origin().has_value());
  }

  // The RenderFrameHost should have changed.
  EXPECT_NE(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());
}

// Ensure that renderer initiated same-site navigations work.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       RendererInitiatedSameSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_FALSE(observer.last_initiator_origin().has_value());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Simulate clicking on a same-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickSameSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(shell()->web_contents()->GetMainFrame()->GetLastCommittedOrigin(),
              observer.last_initiator_origin());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());
}

// Ensure that renderer initiated cross-site navigations work.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       RendererInitiatedCrossSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();
  url::Origin initial_origin = initial_rfh->GetLastCommittedOrigin();

  // Simulate clicking on a cross-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    const char kReplacePortNumber[] =
        "window.domAutomationController.send(setPortNumber(%d));";
    uint16_t port_number = embedded_test_server()->port();
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf(kReplacePortNumber, port_number),
        &success));
    success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickCrossSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(initial_origin, observer.last_initiator_origin().value());
  }

  // The RenderFrameHost should not have changed unless site-per-process is
  // enabled.
  if (AreAllSitesIsolatedForTesting() ||
      IsProactivelySwapBrowsingInstanceEnabled()) {
    EXPECT_NE(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  } else {
    EXPECT_EQ(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  }
}

// Ensure navigation failures are handled.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, FailedNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Now navigate to an unreachable url.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL error_url(embedded_test_server()->GetURL("/close-socket"));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));
    NavigateToURL(shell(), error_url);
    EXPECT_EQ(error_url, observer.last_navigation_url());
    NavigationEntry* entry =
        shell()->web_contents()->GetController().GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  }
}

// Ensure that browser initiated navigations to view-source URLs works.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       ViewSourceNavigation_BrowserInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       url.spec());
  NavigateToURL(shell(), view_source_url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// Ensure that content initiated navigations to view-sources URLs are blocked.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       ViewSourceNavigation_RendererInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL kUrl(embedded_test_server()->GetURL("/simple_links.html"));
  NavigateToURL(shell(), kUrl);
  EXPECT_EQ(kUrl, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: view-source:about:blank"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickViewSourceLink());", &success));
  EXPECT_TRUE(success);
  console_delegate->Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

// Ensure that content initiated navigations to googlechrome: URLs are blocked.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       GoogleChromeNavigation_RendererInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL kUrl(embedded_test_server()->GetURL("/simple_links.html"));
  NavigateToURL(shell(), kUrl);
  EXPECT_EQ(kUrl, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: googlechrome://"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickGoogleChromeLink());",
      &success));
  EXPECT_TRUE(success);
  console_delegate->Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
}

// Ensure that closing a page by running its beforeunload handler doesn't hang
// if there's an ongoing navigation.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, UnloadDuringNavigation) {
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(shell()->web_contents()));
  GURL url("chrome://resources/css/tabs.css");
  NavigationHandleObserver handle_observer(shell()->web_contents(), url);
  shell()->LoadURL(url);
  shell()->web_contents()->DispatchBeforeUnload(false /* auto_cancel */);
  close_observer.Wait();
  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());
}

// Ensure that the referrer of a navigation is properly sanitized.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, SanitizeReferrer) {
  const GURL kInsecureUrl(embedded_test_server()->GetURL("/title1.html"));
  const Referrer kSecureReferrer(
      GURL("https://secure-url.com"),
      network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade);
  ShellNetworkDelegate::SetCancelURLRequestWithPolicyViolatingReferrerHeader(
      true);

  // Navigate to an insecure url with a secure referrer with a policy of no
  // referrer on downgrades. The referrer url should be rewritten right away.
  NavigationController::LoadURLParams load_params(kInsecureUrl);
  load_params.referrer = kSecureReferrer;
  TestNavigationManager manager(shell()->web_contents(), kInsecureUrl);
  shell()->web_contents()->GetController().LoadURLWithParams(load_params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The referrer should have been sanitized.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  ASSERT_TRUE(root->navigation_request());
  EXPECT_EQ(GURL(),
            root->navigation_request()->navigation_handle()->GetReferrer().url);

  // The navigation should commit without being blocked.
  EXPECT_TRUE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();
  EXPECT_EQ(kInsecureUrl, shell()->web_contents()->GetLastCommittedURL());
}

// Test to verify that an exploited renderer process trying to upload a file
// it hasn't been explicitly granted permissions to is correctly terminated.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, PostUploadIllegalFilePath) {
  GURL form_url(
      embedded_test_server()->GetURL("/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  // Prepare a file for the upload form.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  base::RunLoop run_loop;
  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('file').click();"));
  run_loop.Run();

  // Ensure that the process is allowed to access to the chosen file and
  // does not have access to the other file name.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Revoke the access to the file and submit the form. The renderer process
  // should be terminated.
  RenderProcessHostKillWaiter process_kill_waiter(rfh->GetProcess());
  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->RevokeAllPermissionsForFile(rfh->GetProcess()->GetID(),
                                               file_path);

  // Use ExecuteScriptAndExtractBool and respond back to the browser process
  // before doing the actual submission. This will ensure that the process
  // termination is guaranteed to arrive after the response from the executed
  // JavaScript.
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(true);"
      "document.getElementById('file-form').submit();",
      &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(bad_message::ILLEGAL_UPLOAD_PARAMS, process_kill_waiter.Wait());
}

// Test case to verify that redirects to data: URLs are properly disallowed,
// even when invoked through a reload.
// See https://crbug.com/723796.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       VerifyBlockedErrorPageURL_Reload) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Navigate to an URL, which redirects to a data: URL, since it is an
  // unsafe redirect and will result in a blocked navigation and error page.
  GURL redirect_to_blank_url(
      embedded_test_server()->GetURL("/server-redirect?data:text/html,Hello!"));
  EXPECT_FALSE(NavigateToURL(shell(), redirect_to_blank_url));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  TestNavigationObserver reload_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell(), "location.reload()"));
  reload_observer.Wait();

  // The expectation is that the blocked URL is present in the NavigationEntry,
  // and shows up in both GetURL and GetVirtualURL.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->GetURL().SchemeIs(url::kDataScheme));
  EXPECT_EQ(redirect_to_blank_url,
            controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(redirect_to_blank_url,
            controller.GetLastCommittedEntry()->GetVirtualURL());
}

class NavigationDisableWebSecurityTest : public NavigationBrowserTest {
 public:
  NavigationDisableWebSecurityTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Simulate a compromised renderer, otherwise the cross-origin request to
    // file: is blocked.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    NavigationBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test to verify that an exploited renderer process trying to specify a
// non-empty URL for base_url_for_data_url on navigation is correctly
// terminated.
// TODO(nasko): This test case belongs better in
// security_exploit_browsertest.cc, so move it there once PlzNavigate is on
// by default.
IN_PROC_BROWSER_TEST_P(NavigationDisableWebSecurityTest,
                       ValidateBaseUrlForDataUrl) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  GURL data_url("data:text/html,foo");
  base::FilePath file_path = GetTestFilePath("", "simple_page.html");
  GURL file_url = net::FilePathToFileURL(file_path);

  // To get around DataUrlNavigationThrottle. Other attempts at getting around
  // it don't work, i.e.:
  // -if the request is made in a child frame then the frame is torn down
  // immediately on process killing so the navigation doesn't complete
  // -if it's classified as same document, then a DCHECK in
  // NavigationRequest::CreateRendererInitiated fires
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAllowContentInitiatedDataUrlNavigations);
  // Setup a BeginNavigate IPC with non-empty base_url_for_data_url.
  CommonNavigationParams common_params(
      data_url, url::Origin::Create(data_url), Referrer(),
      ui::PAGE_TRANSITION_LINK, FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT,
      NavigationDownloadPolicy(), false /* should_replace_current_entry */,
      file_url, /* base_url_for_data_url */
      GURL() /* history_url_for_data_url */, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now() /* navigation_start */, "GET",
      nullptr /* post_data */, base::Optional<SourceLocation>(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      InitiatorCSPInfo(),
      std::vector<int>() /* initiator_origin_trial_features */,
      std::string() /* href_translate */,
      false /* is_history_navigation_in_new_child_frame */);
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New(
          std::string() /* headers */, net::LOAD_NORMAL,
          false /* skip_service_worker */,
          blink::mojom::RequestContextType::LOCATION,
          blink::WebMixedContentContextType::kBlockable,
          false /* is_form_submission */,
          false /* was_initiated_by_link_click */,
          GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */,
          GURL() /* client_side_redirect_url */,
          base::nullopt /* devtools_initiator_info */);

  // Receiving the invalid IPC message should lead to renderer process
  // termination.
  RenderProcessHostKillWaiter process_kill_waiter(rfh->GetProcess());

  mojom::NavigationClientAssociatedPtr navigation_client;
  if (IsPerNavigationMojoInterfaceEnabled()) {
    auto navigation_client_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&navigation_client);
    rfh->frame_host_binding_for_testing().impl()->BeginNavigation(
        common_params, std::move(begin_params), nullptr,
        navigation_client.PassInterface(), nullptr);
  } else {
    rfh->frame_host_binding_for_testing().impl()->BeginNavigation(
        common_params, std::move(begin_params), nullptr, nullptr, nullptr);
  }
  EXPECT_EQ(bad_message::RFH_BASE_URL_FOR_DATA_URL_SPECIFIED,
            process_kill_waiter.Wait());

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Reload the page to create another renderer process.
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();

  // Make an XHR request to check if the page has access.
  std::string script = base::StringPrintf(
      "var xhr = new XMLHttpRequest()\n"
      "xhr.open('GET', '%s', false);\n"
      "try { xhr.send(); } catch (e) {}\n"
      "window.domAutomationController.send(xhr.responseText);",
      file_url.spec().c_str());
  std::string result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell()->web_contents(), script, &result));
  EXPECT_TRUE(result.empty());
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NavigationDisableWebSecurityTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, BackFollowedByReload) {
  // First, make two history entries.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), url1);
  NavigateToURL(shell(), url2);

  // Then execute a back navigation in Javascript followed by a reload.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.back(); location.reload();"));
  navigation_observer.Wait();

  // The reload should have cancelled the back navigation, and the last
  // committed URL should still be the second URL.
  EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
}

// Test that a navigation response can be entirely fetched, even after the
// NavigationURLLoader has been deleted.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       FetchResponseAfterNavigationURLLoaderDeleted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a new document.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // A NavigationRequest exists at this point.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request());

  // The response's headers are received.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "...");
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();

  // The renderer commits the navigation and the browser deletes its
  // NavigationRequest.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_FALSE(root->navigation_request());

  // The NavigationURLLoader has been deleted by now. Check that the renderer
  // can still receive more bytes.
  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
      shell()->web_contents()->GetMainFrame()));
  response.Send(
      "<script>window.domAutomationController.send('done');</script>");
  std::string done;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
  EXPECT_EQ("\"done\"", done);
}

IN_PROC_BROWSER_TEST_P(NetworkIsolationNavigationBrowserTest,
                       BrowserNavigationNetworkIsolationKey) {
  std::map<GURL, net::NetworkIsolationKey> network_isolation_keys;
  std::map<GURL, network::mojom::UpdateNetworkIsolationKeyOnRedirect>
      update_network_isolation_key_on_redirects;
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin origin = url::Origin::Create(url);

  NavigateAndRecordNetworkIsolationKeys(
      url, url /* final_resource */, false /* from_renderer */,
      &network_isolation_keys, &update_network_isolation_key_on_redirects);
  EXPECT_EQ(net::NetworkIsolationKey(origin, origin),
            network_isolation_keys[url]);
  EXPECT_EQ(network::mojom::UpdateNetworkIsolationKeyOnRedirect::
                kUpdateTopFrameAndFrameOrigin,
            update_network_isolation_key_on_redirects[url]);
}

IN_PROC_BROWSER_TEST_P(NetworkIsolationNavigationBrowserTest,
                       RenderNavigationNetworkIsolationKey) {
  std::map<GURL, net::NetworkIsolationKey> network_isolation_keys;
  std::map<GURL, network::mojom::UpdateNetworkIsolationKeyOnRedirect>
      update_network_isolation_key_on_redirects;
  GURL url(embedded_test_server()->GetURL("/title2.html"));
  url::Origin origin = url::Origin::Create(url);

  NavigateAndRecordNetworkIsolationKeys(
      url, url /* final_resource */, true /* from_renderer */,
      &network_isolation_keys, &update_network_isolation_key_on_redirects);
  EXPECT_EQ(net::NetworkIsolationKey(origin, origin),
            network_isolation_keys[url]);
  EXPECT_EQ(network::mojom::UpdateNetworkIsolationKeyOnRedirect::
                kUpdateTopFrameAndFrameOrigin,
            update_network_isolation_key_on_redirects[url]);
}

IN_PROC_BROWSER_TEST_P(NetworkIsolationNavigationBrowserTest,
                       SubframeNetworkIsolationKey) {
  std::map<GURL, net::NetworkIsolationKey> network_isolation_keys;
  std::map<GURL, network::mojom::UpdateNetworkIsolationKeyOnRedirect>
      update_network_isolation_key_on_redirects;
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL iframe_document = embedded_test_server()->GetURL("/title1.html");
  url::Origin origin = url::Origin::Create(url);
  url::Origin iframe_origin = url::Origin::Create(iframe_document);

  NavigateAndRecordNetworkIsolationKeys(
      url, iframe_document /* final_resource */, false /* from_renderer */,
      &network_isolation_keys, &update_network_isolation_key_on_redirects);
  EXPECT_EQ(net::NetworkIsolationKey(origin, origin),
            network_isolation_keys[url]);
  EXPECT_EQ(network::mojom::UpdateNetworkIsolationKeyOnRedirect::
                kUpdateTopFrameAndFrameOrigin,
            update_network_isolation_key_on_redirects[url]);
  EXPECT_EQ(net::NetworkIsolationKey(origin, iframe_origin),
            network_isolation_keys[iframe_document]);
  EXPECT_EQ(
      network::mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateFrameOrigin,
      update_network_isolation_key_on_redirects[iframe_document]);
}

// Helper class to extract the initiator values from URLLoaderFactory calls
class InitiatorInterceptor {
 public:
  explicit InitiatorInterceptor(const GURL& final_url) {
    // Intercept network requests and record them.
    interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&final_url,
             this](URLLoaderInterceptor::RequestParams* params) -> bool {
              base::AutoLock initiators_lock(lock_);
              (initiators_)[params->url_request.url] =
                  params->url_request.request_initiator;

              if (params->url_request.url == final_url)
                run_loop_.Quit();
              return false;
            }));
  }

  void Run() {
    // Wait until the last resource we care about has been requested.
    run_loop_.Run();
  }

  // This method should be used only if the key already exists in the map.
  const base::Optional<url::Origin>& GetInitiatorForURL(const GURL& url) const {
    auto initiator_iterator = initiators_.find(url);
    DCHECK(initiator_iterator != initiators_.end());

    return initiator_iterator->second;
  }

 private:
  std::map<GURL, base::Optional<url::Origin>> initiators_;
  std::unique_ptr<URLLoaderInterceptor> interceptor_;
  base::Lock lock_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InitiatorInterceptor);
};

// Tests that the initiator is not set for a browser initiated top frame
// navigation.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, BrowserNavigationInitiator) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));

  InitiatorInterceptor test_interceptor(url);

  // Perform the actual navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  test_interceptor.Run();

  ASSERT_FALSE(test_interceptor.GetInitiatorForURL(url).has_value());
}

// Test that the initiator is set to the starting page when a renderer initiated
// navigation goes from the starting page to another page.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, RendererNavigationInitiator) {
  GURL starting_page(embedded_test_server()->GetURL("a.com", "/title1.html"));
  url::Origin starting_page_origin;
  starting_page_origin = starting_page_origin.Create(starting_page);

  NavigateToURL(shell(), starting_page);

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  InitiatorInterceptor test_interceptor(url);

  // Perform the actual navigation.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));
  test_interceptor.Run();

  EXPECT_EQ(starting_page_origin, test_interceptor.GetInitiatorForURL(url));
}

// Test that the initiator is set to the starting page when a sub frame is
// navigated by Javascript from some starting page to another page.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, SubFrameJsNavigationInitiator) {
  GURL starting_page(embedded_test_server()->GetURL("/frame_tree/top.html"));
  NavigateToURL(shell(), starting_page);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // The root and subframe should each have a live RenderFrame.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  InitiatorInterceptor test_interceptor(url);
  std::string script = "location.href='" + url.spec() + "'";

  // Perform the actual navigation.
  EXPECT_TRUE(ExecJs(root->child_at(0)->current_frame_host(), script));
  test_interceptor.Run();

  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  url::Origin starting_page_origin;
  starting_page_origin = starting_page_origin.Create(starting_page);

  EXPECT_EQ(starting_page_origin, test_interceptor.GetInitiatorForURL(url));
}

// Test that the initiator is set to the starting page when a sub frame,
// selected by Id, is navigated by Javascript from some starting page to another
// page.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       SubframeNavigationByTopFrameInitiator) {
  // Go to a page on a.com with an iframe that is on b.com
  GURL starting_page(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  NavigateToURL(shell(), starting_page);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // The root and subframe should each have a live RenderFrame.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  GURL url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  InitiatorInterceptor test_interceptor(url);

  // Perform the actual navigation.
  NavigateIframeToURL(shell()->web_contents(), "child-0", url);
  test_interceptor.Run();

  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  url::Origin starting_page_origin;
  starting_page_origin = starting_page_origin.Create(starting_page);

  EXPECT_EQ(starting_page_origin, test_interceptor.GetInitiatorForURL(url));
}

// Navigation are started in the browser process. After the headers are
// received, the URLLoaderClient is transferred from the browser process to the
// renderer process. This test ensures that when the the URLLoader is deleted
// (in the browser process), the URLLoaderClient (in the renderer process) stops
// properly.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       CancelRequestAfterReadyToCommit) {
  // This test cancels the request using the ResourceDispatchHost. With the
  // NetworkService, it is not used so the request is not canceled.
  // TODO(arthursonzogni): Find a way to cancel a request from the browser
  // with the NetworkService.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Load a new document. Commit the navigation but do not send the full
  //    response's body.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // Let the navigation start.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body> ... ");

  EXPECT_TRUE(navigation_manager.WaitForResponse());
  GlobalRequestID global_id =
      navigation_manager.GetNavigationHandle()->GetGlobalRequestID();
  navigation_manager.ResumeNavigation();

  // The navigation commits successfully. The renderer is waiting for the
  // response's body.
  navigation_manager.WaitForNavigationFinished();

  // 2) The ResourceDispatcherHost cancels the request.
  auto cancel_request = [](GlobalRequestID global_id) {
    ResourceDispatcherHostImpl* rdh =
        static_cast<ResourceDispatcherHostImpl*>(ResourceDispatcherHost::Get());
    rdh->CancelRequest(global_id.child_id, global_id.request_id);
  };
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(cancel_request, global_id));

  // 3) Check that the load stops properly.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

// Data URLs can have a reference fragment like any other URLs. This test makes
// sure it is taken into account.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, DataURLWithReferenceFragment) {
  GURL url("data:text/html,body#foo");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send(document.body.textContent);",
      &body));
  EXPECT_EQ("body", body);

  std::string reference_fragment;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "window.domAutomationController.send(location.hash);",
      &reference_fragment));
  EXPECT_EQ("#foo", reference_fragment);
}

// Regression test for https://crbug.com/796561.
// 1) Start on a document with history.length == 1.
// 2) Create an iframe and call history.pushState at the same time.
// 3) history.back() must work.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       IframeAndPushStateSimultaneously) {
  GURL main_url = embedded_test_server()->GetURL("/simple_page.html");
  GURL iframe_url = embedded_test_server()->GetURL("/hello.html");

  // 1) Start on a new document such that history.length == 1.
  {
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    int history_length;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        shell(), "window.domAutomationController.send(history.length)",
        &history_length));
    EXPECT_EQ(1, history_length);
  }

  // 2) Create an iframe and call history.pushState at the same time.
  {
    TestNavigationManager iframe_navigation(shell()->web_contents(),
                                            iframe_url);
    ExecuteScriptAsync(shell(),
                       "let iframe = document.createElement('iframe');"
                       "iframe.src = '/hello.html';"
                       "document.body.appendChild(iframe);");
    EXPECT_TRUE(iframe_navigation.WaitForRequestStart());

    // The iframe navigation is paused. In the meantime, a pushState navigation
    // begins and ends.
    TestNavigationManager push_state_navigation(shell()->web_contents(),
                                                main_url);
    ExecuteScriptAsync(shell(), "window.history.pushState({}, null);");
    push_state_navigation.WaitForNavigationFinished();

    // The iframe navigation is resumed.
    iframe_navigation.WaitForNavigationFinished();
  }

  // 3) history.back() must work.
  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), "history.back();"));
    navigation_observer.Wait();
  }
}

// Regression test for https://crbug.com/260144
// Back/Forward navigation in an iframe must not stop ongoing XHR.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       IframeNavigationsDoNotStopXHR) {
  // A response for the XHR request. It will be delayed until the end of all the
  // navigations.
  net::test_server::ControllableHttpResponse xhr_response(
      embedded_test_server(), "/xhr");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
      shell()->web_contents()->GetMainFrame()));
  std::string message;

  // 1) Send an XHR.
  ExecuteScriptAsync(
      shell(),
      "let xhr = new XMLHttpRequest();"
      "xhr.open('GET', './xhr', true);"
      "xhr.onabort = () => window.domAutomationController.send('xhr.onabort');"
      "xhr.onerror = () => window.domAutomationController.send('xhr.onerror');"
      "xhr.onload = () => window.domAutomationController.send('xhr.onload');"
      "xhr.send();");

  // 2) Create an iframe and wait for the initial load.
  {
    ExecuteScriptAsync(
        shell(),
        "var iframe = document.createElement('iframe');"
        "iframe.src = './title1.html';"
        "iframe.onload = function() {"
        "   window.domAutomationController.send('iframe.onload');"
        "};"
        "document.body.appendChild(iframe);");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 3) Navigate the iframe elsewhere.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.src = './title2.html';");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 4) history.back() in the iframe.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.contentWindow.history.back()");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 5) history.forward() in the iframe.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.contentWindow.history.forward()");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 6) Wait for the XHR.
  {
    xhr_response.WaitForRequest();
    xhr_response.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Length: 2\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "\r\n"
        "OK");
    xhr_response.Done();
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"xhr.onload\"", message);
  }

  EXPECT_FALSE(dom_message_queue.PopMessage(&message));
}

// Regression test for https://crbug.com/856396.
// Note that original issue for the bug is not applicable anymore, because there
// is no provisional document loader which has not committed yet. We keep the
// modified version of this test to check removing iframe from the load event
// handler.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       ReplacingDocumentLoaderFiresLoadEvent) {
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");
  net::test_server::ControllableHttpResponse iframe_response(
      embedded_test_server(), "/iframe");

  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Load the main document.
  shell()->LoadURL(embedded_test_server()->GetURL("/main_document"));
  main_document_response.WaitForRequest();
  main_document_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<script>"
      "  var detach_iframe = function() {"
      "    var iframe = document.querySelector('iframe');"
      "    iframe.parentNode.removeChild(iframe);"
      "  }"
      "</script>"
      "<body onload='detach_iframe()'>"
      "  <iframe src='/iframe'></iframe>"
      "</body>");
  main_document_response.Done();

  // 2) The iframe starts to load, but the server only have time to send the
  // response's headers, not the response's body. This should commit the
  // iframe's load.
  iframe_response.WaitForRequest();
  iframe_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");

  // 3) In the meantime the iframe navigates elsewhere. It causes the previous
  // DocumentLoader to be replaced by the new one. Removing it may
  // trigger the 'load' event and delete the iframe.
  EXPECT_TRUE(ExecuteScript(
      shell(), "document.querySelector('iframe').src = '/title1.html'"));

  // 4) Finish the original request.
  iframe_response.Done();

  // Wait for the iframe to be deleted and check the renderer process is still
  // alive.
  int iframe_count = 1;
  while (iframe_count != 0) {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        shell(),
        "var iframe_count = document.getElementsByTagName('iframe').length;"
        "window.domAutomationController.send(iframe_count);",
        &iframe_count));
  }
}

class NavigationDownloadBrowserTest : public NavigationBaseBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationBaseBrowserTest::SetUpOnMainThread();

    // Set up a test download directory, in order to prevent prompting for
    // handling downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

// Regression test for https://crbug.com/855033
// 1) A page contains many scripts and DOM elements. It forces the parser to
//    yield CPU to other tasks. That way the response body's data are not fully
//    read when URLLoaderClient::OnComplete(..) is received.
// 2) A script makes the document navigates elsewhere while it is still loading.
//    It cancels the parser of the current document. Due to a bug, the document
//    loader was not marked to be 'loaded' at this step.
// 3) The request for the new navigation starts and it turns out it is a
//    download. The navigation is dropped.
// 4) There are no more possibilities for DidStopLoading() to be sent.
IN_PROC_BROWSER_TEST_P(NavigationDownloadBrowserTest,
                       StopLoadingAfterDroppedNavigation) {
  net::test_server::ControllableHttpResponse main_response(
      embedded_test_server(), "/main");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("/main"));
  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));

  shell()->LoadURL(main_url);
  main_response.WaitForRequest();
  std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";

  // Craft special HTML to make the blink::DocumentParser yield CPU to other
  // tasks. The goal is to ensure the response body datapipe is not fully read
  // when URLLoaderClient::OnComplete() is called.
  // This relies on the  HTMLParserScheduler::ShouldYield() heuristics.
  std::string mix_of_script_and_div = "<script></script><div></div>";
  for (size_t i = 0; i < 10; ++i) {
    mix_of_script_and_div += mix_of_script_and_div;  // Exponential growth.
  }

  std::string navigate_to_download =
      "<script>location.href='" + download_url.spec() + "'</script>";

  main_response.Send(headers + navigate_to_download + mix_of_script_and_div);
  main_response.Done();

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NavigationDownloadBrowserTest,
                         ::testing::Bool());

// Renderer initiated back/forward navigation in beforeunload should not prevent
// the user to navigate away from a website.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, HistoryBackInBeforeUnload) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(
      ExecuteScriptWithoutUserGesture(shell()->web_contents(),
                                      "onbeforeunload = function() {"
                                      "  history.pushState({}, null, '/');"
                                      "  history.back();"
                                      "};"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
}

// Same as 'HistoryBackInBeforeUnload', but wraps history.back() inside
// window.setTimeout(). Thus it is executed "outside" of its beforeunload
// handler and thus avoid basic navigation circumventions.
// Regression test for: https://crbug.com/879965.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       HistoryBackInBeforeUnloadAfterSetTimeout) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(
      ExecuteScriptWithoutUserGesture(shell()->web_contents(),
                                      "onbeforeunload = function() {"
                                      "  history.pushState({}, null, '/');"
                                      "  setTimeout(()=>history.back());"
                                      "};"));
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  auto ipc_observer = base::MakeRefCounted<BrowserMessageObserver>(
      FrameMsgStart, FrameHostMsg_GoToEntryAtOffset::ID);
  static_cast<RenderFrameHostImpl*>(shell()->web_contents()->GetMainFrame())
      ->GetProcess()
      ->AddFilter(ipc_observer.get());

  shell()->LoadURL(url_2);
  ipc_observer->Wait();
  navigation.WaitForNavigationFinished();

  EXPECT_TRUE(navigation.was_successful());
}

// Renderer initiated back/forward navigation can't cancel an ongoing browser
// initiated navigation if it is not user initiated.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       HistoryBackCancelPendingNavigationNoUserGesture) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // 1) A pending browser initiated navigation (omnibox, ...) starts.
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  shell()->LoadURL(url_2);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // 2) history.back() is sent but is not user initiated.
  EXPECT_TRUE(
      ExecuteScriptWithoutUserGesture(shell()->web_contents(),
                                      "history.pushState({}, null, '/');"
                                      "history.back();"));

  // 3) The first pending navigation is not canceled and can continue.
  navigation.WaitForNavigationFinished();  // Resume navigation.
  EXPECT_TRUE(navigation.was_successful());
}

// Renderer initiated back/forward navigation can cancel an ongoing browser
// initiated navigation if it is user initiated.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       HistoryBackCancelPendingNavigationUserGesture) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // 1) A pending browser initiated navigation (omnibox, ...) starts.
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  shell()->LoadURL(url_2);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // 2) history.back() is sent and is user initiated.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.pushState({}, null, '/');"
                            "history.back();"));

  // 3) Check the first pending navigation has been canceled.
  navigation.WaitForNavigationFinished();  // Resume navigation.
  EXPECT_FALSE(navigation.was_successful());
}

namespace {

// Checks whether the given urls are requested, and that GetPreviewsState()
// returns the appropriate value when the Previews are set.
class PreviewsStateContentBrowserClient : public ContentBrowserClient {
 public:
  explicit PreviewsStateContentBrowserClient(const GURL& main_frame_url)
      : main_frame_url_(main_frame_url),
        main_frame_url_seen_(false),
        previews_state_(PREVIEWS_OFF),
        determine_allowed_previews_called_(false),
        determine_committed_previews_called_(false) {}

  ~PreviewsStateContentBrowserClient() override {}

  content::PreviewsState DetermineAllowedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const GURL& current_navigation_url) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    EXPECT_FALSE(determine_allowed_previews_called_);
    determine_allowed_previews_called_ = true;
    main_frame_url_seen_ = true;
    EXPECT_EQ(main_frame_url_, current_navigation_url);
    return previews_state_;
  }

  content::PreviewsState DetermineCommittedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_EQ(previews_state_, initial_state);
    determine_committed_previews_called_ = true;
    return initial_state;
  }

  void SetClient() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::SetBrowserClientForTesting(this);
  }

  void Reset(PreviewsState previews_state) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    main_frame_url_seen_ = false;
    previews_state_ = previews_state;
    determine_allowed_previews_called_ = false;
  }

  void CheckResourcesRequested() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(determine_allowed_previews_called_);
    EXPECT_TRUE(determine_committed_previews_called_);
    EXPECT_TRUE(main_frame_url_seen_);
  }

 private:
  const GURL main_frame_url_;

  bool main_frame_url_seen_;
  PreviewsState previews_state_;
  bool determine_allowed_previews_called_;
  bool determine_committed_previews_called_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsStateContentBrowserClient);
};

}  // namespace

class PreviewsStateBrowserTest : public ContentBrowserTest {
 public:
  ~PreviewsStateBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    client_.reset(new PreviewsStateContentBrowserClient(
        embedded_test_server()->GetURL("/title1.html")));

    client_->SetClient();
  }

  void Reset(PreviewsState previews_state) { client_->Reset(previews_state); }

  void CheckResourcesRequested() { client_->CheckResourcesRequested(); }

 private:
  std::unique_ptr<PreviewsStateContentBrowserClient> client_;
};

// Test that navigating calls GetPreviewsState returning PREVIEWS_OFF.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnablePreviewsOff) {
  // Navigate with No Previews.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/title1.html"), 1);
  CheckResourcesRequested();
}

// Ensure the renderer process doesn't send too many IPC to the browser process
// when history.pushState() and history.back() are called in a loop.
// Failing to do so causes the browser to become unresponsive.
// See https://crbug.com/882238
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, IPCFlood_GoToEntryAtOffset) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Throttling navigation to prevent the browser from hanging. See "
          "https://crbug.com/882238. Command line switch "
          "--disable-ipc-flooding-protection can be used to bypass the "
          "protection"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(ExecuteScript(shell(), R"(
    for(let i = 0; i<1000; ++i) {
      history.pushState({},"page 2", "bar.html");
      history.back();
    }
  )"));

  console_delegate->Wait();
}

// Ensure the renderer process doesn't send too many IPC to the browser process
// when doing a same-document navigation is requested in a loop.
// Failing to do so causes the browser to become unresponsive.
// TODO(arthursonzogni): Make the same test, but when the navigation is
// requested from a remote frame.
// See https://crbug.com/882238
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, IPCFlood_Navigation) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Throttling navigation to prevent the browser from hanging. See "
          "https://crbug.com/882238. Command line switch "
          "--disable-ipc-flooding-protection can be used to bypass the "
          "protection"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(ExecuteScript(shell(), R"(
    for(let i = 0; i<1000; ++i) {
      location.href = "#" + i;
      ++i;
    }
  )"));

  console_delegate->Wait();
}

// TODO(http://crbug.com/632514): This test currently expects opener downloads
// go through and UMA is logged, but when the linked bug is resolved the
// download should be disallowed.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, OpenerNavigation_DownloadPolicy) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());
  ShellDownloadManagerDelegate* delegate =
      static_cast<ShellDownloadManagerDelegate*>(
          shell()
              ->web_contents()
              ->GetBrowserContext()
              ->GetDownloadManagerDelegate());
  delegate->SetDownloadBehaviorForTesting(download_dir.GetPath());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  WebContents* opener = shell()->web_contents();

  // Open a popup.
  bool opened = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener, "window.domAutomationController.send(!!window.open());",
      &opened));
  EXPECT_TRUE(opened);
  EXPECT_EQ(2u, Shell::windows().size());

  // Using the popup, navigate its opener to a download.
  base::HistogramTester histograms;
  WebContents* popup = Shell::windows()[1]->web_contents();
  EXPECT_NE(popup, opener);
  DownloadTestObserverInProgress observer(
      BrowserContext::GetDownloadManager(opener->GetBrowserContext()),
      1 /* wait_count */);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      popup,
      "window.opener.location ='data:html/text;base64,'+btoa('payload');"));
  observer.WaitForFinished();

  // Implies NavigationDownloadType::kOpenerCrossOrigin has 0 count.
  histograms.ExpectUniqueSample("Navigation.DownloadPolicy.LogPerPolicyApplied",
                                NavigationDownloadType::kNoGesture, 1);
}

// A variation of the OpenerNavigation_DownloadPolicy test above, but uses a
// cross-origin URL for the popup window.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest,
                       CrossOriginOpenerNavigation_DownloadPolicy) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());
  ShellDownloadManagerDelegate* delegate =
      static_cast<ShellDownloadManagerDelegate*>(
          shell()
              ->web_contents()
              ->GetBrowserContext()
              ->GetDownloadManagerDelegate());
  delegate->SetDownloadBehaviorForTesting(download_dir.GetPath());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/title1.html"));
  WebContents* opener = shell()->web_contents();

  // Open a popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(EvalJs(opener, JsReplace("!!window.open($1);",
                                       embedded_test_server()->GetURL(
                                           "bar.com", "/title1.html")))
                  .ExtractBool());
  Shell* new_shell = shell_observer.GetShell();
  EXPECT_EQ(2u, Shell::windows().size());

  // Wait for the navigation in the popup to complete, so the origin of the
  // document will be correct.
  WebContents* popup = new_shell->web_contents();
  EXPECT_NE(popup, opener);
  EXPECT_TRUE(WaitForLoadStop(popup));

  // Using the popup, navigate its opener to a download.
  base::HistogramTester histograms;
  const GURL data_url("data:html/text;base64,cGF5bG9hZA==");
  TestNavigationManager manager(shell()->web_contents(), data_url);
  EXPECT_TRUE(
      ExecuteScript(popup, base::StringPrintf("window.opener.location ='%s'",
                                              data_url.spec().c_str())));
  manager.WaitForNavigationFinished();

  EXPECT_FALSE(manager.was_successful());

  // Navigations downloads that go through ResourceDispatcherHost do not trigger
  // metrics collection, since the "cancellation reason" is collapsed to a
  // boolean before the navigation turns into a download. Just expect metrics
  // when the network service is enabled.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    histograms.ExpectBucketCount(
        "Navigation.DownloadPolicy.LogPerPolicyApplied",
        NavigationDownloadType::kOpenerCrossOrigin, 1);
  }
}

// Regression test for https://crbug.com/872284.
// A NavigationThrottle cancels a download in WillProcessResponse.
// The navigation request must be canceled and it must also cancel the network
// request. Failing to do so resulted in the network socket being leaked.
IN_PROC_BROWSER_TEST_P(NavigationDownloadBrowserTest,
                       CancelDownloadOnResponseStarted) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  // Block every iframe in WillProcessResponse.
  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::CANCEL_AND_IGNORE);

            return throttle;
          }));

  // Insert enough iframes so that if sockets are not properly released: there
  // will not be enough of them to complete all navigations. As of today, only 6
  // sockets can be used simultaneously. So using 7 iframes is enough. This test
  // uses 33 as a margin.
  EXPECT_TRUE(ExecJs(shell(), R"(
    for(let i = 0; i<33; ++i) {
      let iframe = document.createElement('iframe');
      iframe.src = './download-test1.lib'
      document.body.appendChild(iframe);
    }
  )"));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

// Add header on redirect.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest, AddRequestHeaderOnRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            NavigationHandleImpl* handle_impl =
                static_cast<NavigationHandleImpl*>(handle);
            throttle->SetCallback(TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                                  base::BindLambdaForTesting([handle_impl]() {
                                    handle_impl->SetRequestHeader(
                                        "header_name", "header_value");
                                  }));
            return throttle;
          }));

  // 1) There is no "header_name" header in the initial request.
  shell()->LoadURL(embedded_test_server()->GetURL("/doc"));
  response_1.WaitForRequest();
  EXPECT_FALSE(
      base::Contains(response_1.http_request()->headers, "header_name"));
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  // 2) The header is added to the second request after the redirect.
  response_2.WaitForRequest();
  EXPECT_EQ("header_value",
            response_2.http_request()->headers.at("header_name"));
}

// Add header on request start, modify it on redirect.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       AddRequestHeaderModifyOnRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            NavigationHandleImpl* handle_impl =
                static_cast<NavigationHandleImpl*>(handle);
            throttle->SetCallback(TestNavigationThrottle::WILL_START_REQUEST,
                                  base::BindLambdaForTesting([handle_impl]() {
                                    handle_impl->SetRequestHeader(
                                        "header_name", "header_value");
                                  }));
            throttle->SetCallback(TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                                  base::BindLambdaForTesting([handle_impl]() {
                                    handle_impl->SetRequestHeader(
                                        "header_name", "other_value");
                                  }));
            return throttle;
          }));

  // 1) The header is added to the initial request.
  shell()->LoadURL(embedded_test_server()->GetURL("/doc"));
  response_1.WaitForRequest();
  EXPECT_EQ("header_value",
            response_1.http_request()->headers.at("header_name"));
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  // 2) The header is modified in the second request after the redirect.
  response_2.WaitForRequest();
  EXPECT_EQ("other_value",
            response_2.http_request()->headers.at("header_name"));
}

// Add header on request start, remove it on redirect.
IN_PROC_BROWSER_TEST_P(NavigationBaseBrowserTest,
                       AddRequestHeaderRemoveOnRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            NavigationHandleImpl* handle_impl =
                static_cast<NavigationHandleImpl*>(handle);
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetCallback(TestNavigationThrottle::WILL_START_REQUEST,
                                  base::BindLambdaForTesting([handle_impl]() {
                                    handle_impl->SetRequestHeader(
                                        "header_name", "header_value");
                                  }));
            throttle->SetCallback(
                TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                base::BindLambdaForTesting([handle_impl]() {
                  handle_impl->RemoveRequestHeader("header_name");
                }));
            return throttle;
          }));

  // 1) The header is added to the initial request.
  shell()->LoadURL(embedded_test_server()->GetURL("/doc"));
  response_1.WaitForRequest();
  EXPECT_EQ("header_value",
            response_1.http_request()->headers.at("header_name"));
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  // 2) The header is removed from the second request after the redirect.
  response_2.WaitForRequest();
  EXPECT_FALSE(
      base::Contains(response_2.http_request()->headers, "header_name"));
}

struct NewWebContentsData {
  NewWebContentsData() = default;
  NewWebContentsData(NewWebContentsData&& other)
      : new_web_contents(std::move(other.new_web_contents)),
        manager(std::move(other.manager)) {}

  std::unique_ptr<WebContents> new_web_contents;
  std::unique_ptr<TestNavigationManager> manager;
};

class CreateWebContentsOnCrashObserver : public NotificationObserver {
 public:
  CreateWebContentsOnCrashObserver(const GURL& url,
                                   WebContents* first_web_contents)
      : url_(url), first_web_contents_(first_web_contents) {}

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    EXPECT_EQ(content::NOTIFICATION_RENDERER_PROCESS_CLOSED, type);

    // Only do this once in the test.
    if (observed_)
      return;
    observed_ = true;

    WebContents::CreateParams new_contents_params(
        first_web_contents_->GetBrowserContext(),
        first_web_contents_->GetSiteInstance());
    data_.new_web_contents = WebContents::Create(new_contents_params);
    data_.manager = std::make_unique<TestNavigationManager>(
        data_.new_web_contents.get(), url_);
    NavigationController::LoadURLParams load_params(url_);
    data_.new_web_contents->GetController().LoadURLWithParams(load_params);
  }

  NewWebContentsData TakeNewWebContentsData() { return std::move(data_); }

 private:
  NewWebContentsData data_;
  bool observed_ = false;

  GURL url_;
  WebContents* first_web_contents_;

  ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;

  DISALLOW_COPY_AND_ASSIGN(CreateWebContentsOnCrashObserver);
};

// This test simulates android webview's behavior in apps that handle
// renderer crashes by synchronously creating a new WebContents and loads
// the same page again. This reenters into content code.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, WebViewRendererKillReload) {
  // Webview is limited to one renderer.
  RenderProcessHost::SetMaxRendererProcessCount(1u);

  // Load a page into first webview.
  auto* web_contents = shell()->web_contents();
  GURL url(embedded_test_server()->GetURL("/simple_links.html"));
  {
    TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(NavigateToURL(web_contents, url));
    EXPECT_EQ(url, observer.last_navigation_url());
  }

  // Install a crash observer that synchronously creates and loads a new
  // WebContents. Then crash the renderer which triggers the observer.
  CreateWebContentsOnCrashObserver crash_observer(url, web_contents);
  content::NotificationRegistrar notification_registrar;
  notification_registrar.Add(&crash_observer,
                             content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                             content::NotificationService::AllSources());
  NavigateToURLBlockUntilNavigationsComplete(web_contents, GetWebUIURL("crash"),
                                             1);

  // Wait for navigation in new WebContents to finish.
  NewWebContentsData data = crash_observer.TakeNewWebContentsData();
  data.manager->WaitForNavigationFinished();

  // Test passes if renderer is still alive.
  EXPECT_TRUE(ExecJs(data.new_web_contents.get(), "true;"));
  EXPECT_TRUE(data.new_web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(url, data.new_web_contents->GetMainFrame()->GetLastCommittedURL());
}

// Test NavigationRequest::CheckAboutSrcDoc()
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, BlockedSrcDocBrowserInitiated) {
  // 1. Main frame navigations to about:srcdoc and its variations are blocked.
  for (const char* url :
       {"about:srcdoc", "about:srcdoc?foo", "about:srcdoc#foo"}) {
    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL(url));
    EXPECT_FALSE(NavigateToURL(shell(), GURL(url)));
    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_TRUE(handle_observer.is_error());
    EXPECT_EQ(net::ERR_INVALID_URL, handle_observer.net_error_code());
  }

  // 2. Subframe navigations to variations of about:srcdoc are blocked.
  for (const char* url : {"about:srcdoc?foo", "about:srcdoc#foo"}) {
    GURL main_url =
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL(url));
    shell()->LoadURLForFrame(GURL(url), "child-name-0",
                             ui::PAGE_TRANSITION_FORWARD_BACK);
    WaitForLoadStop(shell()->web_contents());
    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_TRUE(handle_observer.is_error());
    EXPECT_EQ(net::ERR_INVALID_URL, handle_observer.net_error_code());
  }

  // 3. Subframe navigation to about:srcdoc are not blocked.
  {
    GURL main_url =
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL("about::srcdoc"));
    shell()->LoadURLForFrame(GURL("about::srcdoc"), "child-name-0",
                             ui::PAGE_TRANSITION_FORWARD_BACK);
    WaitForLoadStop(shell()->web_contents());
    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_FALSE(handle_observer.is_error());
    EXPECT_EQ(net::OK, handle_observer.net_error_code());
  }
}

// Test NavigationRequest::CheckAboutSrcDoc().
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, BlockedSrcDocRendererInitiated) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  FrameTreeNode* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root();

  // 1. Main frame navigations to about:srcdoc and its variations are blocked.
  for (const char* url :
       {"about:srcdoc", "about:srcdoc?foo", "about:srcdoc#foo"}) {
    DidStartNavigationObserver start_observer(shell()->web_contents());
    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL(url));
    // TODO(arthursonzogni): It shouldn't be possible to navigate to
    // about:srcdoc by executing location.href= "about:srcdoc". Other web
    // browsers like Firefox aren't allowing this.
    EXPECT_TRUE(ExecJs(main_frame, JsReplace("location.href = $1", url)));
    start_observer.Wait();
    WaitForLoadStop(shell()->web_contents());
    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_TRUE(handle_observer.is_error());
    EXPECT_EQ(net::ERR_INVALID_URL, handle_observer.net_error_code());
  }

  // 2. Subframe navigations to variations of about:srcdoc are blocked.
  for (const char* url : {"about:srcdoc?foo", "about:srcdoc#foo"}) {
    GURL main_url =
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    DidStartNavigationObserver start_observer(shell()->web_contents());
    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL(url));
    FrameTreeNode* subframe = main_frame->child_at(0);
    // TODO(arthursonzogni): It shouldn't be possible to navigate to
    // about:srcdoc by executing location.href= "about:srcdoc". Other web
    // browsers like Firefox aren't allowing this.
    EXPECT_TRUE(ExecJs(subframe, JsReplace("location.href = $1", url)));
    start_observer.Wait();
    WaitForLoadStop(shell()->web_contents());
    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_TRUE(handle_observer.is_error());
    EXPECT_EQ(net::ERR_INVALID_URL, handle_observer.net_error_code());
  }

  // 3. Subframe navigation to about:srcdoc are not blocked.
  {
    GURL main_url =
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    DidStartNavigationObserver start_observer(shell()->web_contents());
    NavigationHandleObserver handle_observer(shell()->web_contents(),
                                             GURL("about:srcdoc"));
    FrameTreeNode* subframe = main_frame->child_at(0);
    // TODO(arthursonzogni): It shouldn't be possible to navigate to
    // about:srcdoc by executing location.href="about:srcdoc". Other web
    // browsers like Firefox aren't allowing this.
    EXPECT_TRUE(ExecJs(subframe, JsReplace("location.href = 'about:srcdoc'")));

    start_observer.Wait();
    WaitForLoadStop(shell()->web_contents());

    EXPECT_TRUE(handle_observer.has_committed());
    EXPECT_FALSE(handle_observer.is_error());
    EXPECT_EQ(net::OK, handle_observer.net_error_code());
  }
}

// Test renderer initiated navigations to about:srcdoc are routed through the
// browser process. It means RenderFrameHostImpl::BeginNavigation() is called.
IN_PROC_BROWSER_TEST_P(NavigationBrowserTest, AboutSrcDocUsesBeginNavigation) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  // If DidStartNavigation is called before DidCommitProvisionalLoad, then it
  // means the navigation was driven by the browser process, otherwise by the
  // renderer process. This tests it was driven by the browser process:
  InterceptAndCancelDidCommitProvisionalLoad interceptor(
      shell()->web_contents());
  DidStartNavigationObserver observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
    let iframe = document.createElement("iframe");
    iframe.srcdoc = "foo"
    document.body.appendChild(iframe);
  )"));

  observer.Wait();      // BeginNavigation is called.
  interceptor.Wait(1);  // DidCommitNavigation is called.
}

class TextFragmentAnchorBrowserTest : public NavigationBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NavigationBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "TextFragmentIdentifiers");
  }

  // Simulates a click on the middle of the DOM element with the given |id|.
  void ClickElementWithId(WebContents* web_contents, const std::string& id) {
    // Get the center coordinates of the DOM element.
    const int x = EvalJs(web_contents,
                         JsReplace("const bounds = "
                                   "document.getElementById($1)."
                                   "getBoundingClientRect();"
                                   "Math.floor(bounds.left + bounds.width / 2)",
                                   id))
                      .ExtractInt();
    const int y = EvalJs(web_contents,
                         JsReplace("const bounds = "
                                   "document.getElementById($1)."
                                   "getBoundingClientRect();"
                                   "Math.floor(bounds.top + bounds.height / 2)",
                                   id))
                      .ExtractInt();

    SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                         gfx::Point(x, y));
  }

  void WaitForPageLoad(WebContents* contents) {
    EXPECT_TRUE(WaitForLoadStop(contents));
    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetMainFrame()));
  }
};

IN_PROC_BROWSER_TEST_P(TextFragmentAnchorBrowserTest, EnabledOnUserNavigation) {
  GURL url(embedded_test_server()->GetURL("/target_text_link.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#targetText=text"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContents* main_contents = shell()->web_contents();
  TestNavigationObserver observer(main_contents);
  RenderFrameSubmissionObserver frame_observer(main_contents);
  ClickElementWithId(main_contents, "link");
  observer.Wait();
  EXPECT_EQ(target_text_url, main_contents->GetLastCommittedURL());

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(false);
  EXPECT_FALSE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

IN_PROC_BROWSER_TEST_P(TextFragmentAnchorBrowserTest,
                       EnabledOnBrowserNavigation) {
  GURL url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#targetText=text"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(false);
  EXPECT_FALSE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

IN_PROC_BROWSER_TEST_P(TextFragmentAnchorBrowserTest,
                       EnabledOnUserGestureScriptNavigation) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#targetText=text"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContents* main_contents = shell()->web_contents();
  TestNavigationObserver observer(main_contents);
  RenderFrameSubmissionObserver frame_observer(main_contents);

  // ExecuteScript executes with a user gesture
  EXPECT_TRUE(ExecuteScript(main_contents,
                            "location = '" + target_text_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(target_text_url, main_contents->GetLastCommittedURL());

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(false);
  EXPECT_FALSE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

IN_PROC_BROWSER_TEST_P(TextFragmentAnchorBrowserTest,
                       DisabledOnScriptNavigation) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#targetText=text"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContents* main_contents = shell()->web_contents();
  TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      main_contents, "location = '" + target_text_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(target_text_url, main_contents->GetLastCommittedURL());

  WaitForPageLoad(main_contents);

  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  EXPECT_TRUE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

IN_PROC_BROWSER_TEST_P(TextFragmentAnchorBrowserTest,
                       DisabledOnScriptHistoryNavigation) {
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#targetText=text"));
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), target_text_url));

  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(false);

  // Scroll the page back to top so scroll restoration does not scroll the
  // target back into view.
  EXPECT_TRUE(ExecuteScript(main_contents, "window.scrollTo(0, 0)"));
  frame_observer.WaitForScrollOffsetAtTop(true);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(main_contents, "history.back()"));
  observer.Wait();
  EXPECT_EQ(target_text_url, main_contents->GetLastCommittedURL());

  WaitForPageLoad(main_contents);

  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  EXPECT_TRUE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         TextFragmentAnchorBrowserTest,
                         ::testing::Bool());

}  // namespace content
