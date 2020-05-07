// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

network::CrossOriginOpenerPolicy CoopSameOrigin() {
  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginAllowPopups() {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  return coop;
}

network::CrossOriginOpenerPolicy CoopUnsafeNone() {
  network::CrossOriginOpenerPolicy coop;
  // Using the default value.
  return coop;
}

class CrossOriginOpenerPolicyBrowserTest : public ContentBrowserTest {
 public:
  CrossOriginOpenerPolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::Feature> features;
    feature_list_.InitWithFeatures(
        {network::features::kCrossOriginOpenerPolicy,
         network::features::kCrossOriginEmbedderPolicy},
        {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOrigin) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(CoopSameOrigin());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(), CoopSameOrigin());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(CoopSameOriginAllowPopups());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInherit) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(CoopSameOrigin());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(), CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_F(
    CrossOriginOpenerPolicyBrowserTest,
    NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(CoopSameOrigin());

  ShellAddedObserver new_shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  Shell* new_shell = new_shell_observer.GetShell();
  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  scoped_refptr<SiteInstance> main_frame_site_instance(
      main_frame->GetSiteInstance());
  scoped_refptr<SiteInstance> iframe_site_instance(iframe->GetSiteInstance());
  scoped_refptr<SiteInstance> popup_site_instance(
      popup_frame->GetSiteInstance());

  ASSERT_TRUE(main_frame_site_instance);
  ASSERT_TRUE(iframe_site_instance);
  ASSERT_TRUE(popup_site_instance);
  EXPECT_FALSE(main_frame_site_instance->IsRelatedSiteInstance(
      popup_site_instance.get()));
  EXPECT_FALSE(
      iframe_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));

  // Check that `window.opener` is not set.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success) << "window.opener is set";
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NetworkErrorOnSandboxedPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_sandbox_popup.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(
      iframe, "window.open('/cross-origin-opener-policy_same-origin.html')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  EXPECT_EQ(
      popup_webcontents->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_ERROR);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NoNetworkErrorOnSandboxedDocuments) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_csp_sandboxed.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  EXPECT_NE(current_frame_host()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kNone)
      << "Document should be sandboxed.";

  GURL next_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  EXPECT_TRUE(NavigateToURL(shell(), next_page));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);
}

class CrossOriginPolicyHeadersObserver : public WebContentsObserver {
 public:
  explicit CrossOriginPolicyHeadersObserver(
      WebContents* web_contents,
      network::mojom::CrossOriginEmbedderPolicyValue expected_coep,
      network::CrossOriginOpenerPolicy expected_coop)
      : WebContentsObserver(web_contents),
        expected_coep_(expected_coep),
        expected_coop_(expected_coop) {}

  ~CrossOriginPolicyHeadersObserver() override = default;

  void DidRedirectNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_opener_policy == expected_coop_);
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_opener_policy == expected_coop_);
  }

 private:
  network::mojom::CrossOriginEmbedderPolicyValue expected_coep_;
  network::CrossOriginOpenerPolicy expected_coop_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       RedirectsParseCoopAndCoepHeaders) {
  GURL redirect_initial_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_initial.html"));
  GURL redirect_final_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_final.html"));

  CrossOriginPolicyHeadersObserver obs(
      web_contents(),
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      CoopSameOrigin());

  EXPECT_TRUE(
      NavigateToURL(shell(), redirect_initial_page, redirect_final_page));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       CoopIsIgnoredOverHttp) {
  GURL non_coop_page(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(embedded_test_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  EXPECT_EQ(current_frame_host()->GetSiteInstance(), initial_site_instance);
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       CoopIsIgnoredOnIframes) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL iframe_navigation_url(https_server()->GetURL(
      "b.com", "/cross-origin-opener-policy_same-origin.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();
  FrameTreeNode* iframe_ftn = main_rfh->child_at(0);
  RenderFrameHostImpl* iframe_rfh = iframe_ftn->current_frame_host();
  SiteInstanceImpl* non_coop_iframe_site_instance =
      iframe_rfh->GetSiteInstance();

  // Navigate the iframe same-origin to a document with the COOP header. The
  // header must be ignored in iframes.
  NavigateFrameToURL(iframe_ftn, iframe_navigation_url);
  iframe_rfh = iframe_ftn->current_frame_host();

  // We expect the navigation to have used the same SiteInstance that was used
  // in the first place since they are same origin and COOP is ignored.
  EXPECT_EQ(iframe_rfh->GetLastCommittedURL(), iframe_navigation_url);
  EXPECT_EQ(iframe_rfh->GetSiteInstance(), non_coop_iframe_site_instance);

  // TODO(pmeuleman, ahemery): Don't store COOP on subframes as it will not be
  // used anyway.
  EXPECT_EQ(iframe_rfh->cross_origin_opener_policy(), CoopSameOrigin());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NonCoopPageCrashIntoCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  // Test a crash before the navigation.
  {
    // Navigate to a non coop page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    GURL non_coop_cross_site_page(
        https_server()->GetURL("b.com", "/title1.html"));
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // The COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a non coop page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    GURL non_coop_cross_site_page(
        https_server()->GetURL("b.com", "/title1.html"));

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the COOP page.
    coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // The COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoNonCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                       "var iframe = document.createElement('iframe');"
                       "iframe.src = '" +
                           cross_site_iframe.spec() +
                           "';"
                           "document.body.appendChild(iframe);"));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopUnsafeNone());

    // The non COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                       "var iframe = document.createElement('iframe');"
                       "iframe.src = '" +
                           cross_site_iframe.spec() +
                           "';"
                           "document.body.appendChild(iframe);"));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the non COOP page.
    non_coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(non_coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopUnsafeNone());

    // The non COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                       "var iframe = document.createElement('iframe');"
                       "iframe.src = '" +
                           cross_site_iframe.spec() +
                           "';"
                           "document.body.appendChild(iframe);"));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // TODO(pmeuleman): The COOP page should still have RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                       "var iframe = document.createElement('iframe');"
                       "iframe.src = '" +
                           cross_site_iframe.spec() +
                           "';"
                           "document.body.appendChild(iframe);"));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the COOP page.
    coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(coop_navigation.was_successful());
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // TODO(pmeuleman): The COOP page should still have RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreRemovedWhenCrossingCoopBoundary) {
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  RenderFrameHostManager* main_window_rfhm =
      web_contents()->GetFrameTree()->root()->render_manager();
  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
  EXPECT_EQ(main_window_rfhm->GetProxyCount(), 0u);

  Shell* popup_shell = OpenPopup(shell(), coop_page, "");

  // The main frame should not have the popup referencing it.
  EXPECT_EQ(main_window_rfhm->GetProxyCount(), 0u);

  // It should not have any other related SiteInstance.
  EXPECT_EQ(
      current_frame_host()->GetSiteInstance()->GetRelatedActiveContentsCount(),
      1u);

  // The popup should not have the main frame referencing it.
  FrameTreeNode* popup =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  RenderFrameHostManager* popup_rfhm = popup->render_manager();
  EXPECT_EQ(popup_rfhm->GetProxyCount(), 0u);

  // The popup should have an empty opener.
  EXPECT_FALSE(popup->opener());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreKeptWhenNavigatingFromCoopToCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  // Ensure it has a RenderFrameHostProxy for another cross-site page.
  Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
  GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                          cross_site_iframe);
  EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.src = '" +
                         cross_site_iframe.spec() +
                         "';"
                         "document.body.appendChild(iframe);"));
  iframe_navigation.WaitForNavigationFinished();
  EXPECT_EQ(
      web_contents()->GetFrameTree()->root()->render_manager()->GetProxyCount(),
      1u);

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));

  // The COOP page should still have a RenderFrameProxyHost.
  EXPECT_EQ(
      web_contents()->GetFrameTree()->root()->render_manager()->GetProxyCount(),
      1u);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       IsolateInNewProcessDespiteLimitReached) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page.
  GURL starting_page(https_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Open a popup with CrossOriginOpenerPolicy and CrossOriginEmbedderPolicy
  // set.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "window.open('/page_with_coop_and_coep.html')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  // The page and its popup should be in different processes even though the
  // process limit was reached.
  // TODO(clamy, pmeuleman, ahemery): The assert below should be false once we
  // fix the process reuse for COOP.
  EXPECT_EQ(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NoProcessReuseForCOOPProcesses) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page with CrossOriginOpenerPolicy and
  // CrossOriginEmbedderPolicy set.
  GURL starting_page(
      https_server()->GetURL("a.com", "/page_with_coop_and_coep.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Open a popup without CrossOriginOpenerPolicy and CrossOriginEmbedderPolicy
  // set.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.open('/title1.html')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  // The page and its popup should be in different processes even though the
  // process limit was reached.
  // TODO(clamy, pmeuleman, ahemery): The assert below should be false once we
  // fix the process reuse for COOP.
  EXPECT_EQ(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());

  // Navigate to a new page without COOP and COEP. Because of process reuse, it
  // is placed in the popup process.
  GURL final_page(https_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), final_page));
  EXPECT_EQ(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       SpeculativeRfhsAndCoop) {
  GURL non_coop_page(https_server()->GetURL("/title1.html"));
  GURL coop_page(https_server()->GetURL("/page_with_coop_and_coep.html"));

  // Non-COOP into non-COOP.
  {
    // Start on a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    non_coop_navigation.WaitForNavigationFinished();

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  }

  // Non-COOP into COOP.
  {
    // Start on a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    coop_navigation.WaitForNavigationFinished();

    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  }

  // COOP into non-COOP.
  {
    // Start on a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    non_coop_navigation.WaitForNavigationFinished();

    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  }

  // COOP into COOP.
  {
    // Start on a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    coop_navigation.WaitForNavigationFinished();

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  }
}

}  // namespace

}  // namespace content
