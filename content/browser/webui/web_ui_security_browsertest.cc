// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

class WebUISecurityTest : public ContentBrowserTest {
 public:
  WebUISecurityTest() { WebUIControllerFactory::RegisterFactory(&factory_); }

  ~WebUISecurityTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUISecurityTest);
};

// Verify chrome-untrusted:// have no bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, UntrustedNoBindings) {
  auto* web_contents = shell()->web_contents();
  AddUntrustedDataSource(web_contents->GetBrowserContext(), "test-host");

  const GURL untrusted_url(GetChromeUntrustedUIURL("test-host/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents, untrusted_url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(0, shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which does not have any bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, NoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=0"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(0, shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has WebUI bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, MojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has both WebUI and Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIAndMojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI |
                                                 BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI | BINDINGS_POLICY_MOJO_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Verify that reloading a WebUI document or navigating between documents on
// the same WebUI will result in using the same SiteInstance and will not
// create a new WebUI instance.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIReuse) {
  GURL test_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Capture the SiteInstance and WebUI used in the first navigation to compare
  // with the ones used after the reload.
  scoped_refptr<SiteInstance> initial_site_instance =
      root->current_frame_host()->GetSiteInstance();
  WebUI* initial_web_ui = root->current_frame_host()->web_ui();

  // Reload the document and check that SiteInstance and WebUI are reused.
  TestFrameNavigationObserver observer(root);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(test_url, observer.last_committed_url());

  EXPECT_EQ(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, root->current_frame_host()->web_ui());

  // Navigate to another document on the same WebUI and check that SiteInstance
  // and WebUI are reused.
  GURL next_url(GetWebUIURL("web-ui/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), next_url));

  EXPECT_EQ(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, root->current_frame_host()->web_ui());
}

// Verify that a WebUI can add a subframe for its own WebUI.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUISameSiteSubframe) {
  GURL test_url(GetWebUIURL("web-ui/page_with_blank_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());

  TestFrameNavigationObserver observer(root->child_at(0));
  GURL subframe_url(GetWebUIURL("web-ui/title1.html?noxfo=true"));
  NavigateFrameToURL(root->child_at(0), subframe_url);

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(subframe_url, observer.last_committed_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      GetWebUIURL("web-ui"),
      root->child_at(0)->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // The subframe should have its own WebUI object different from the parent
  // frame.
  EXPECT_NE(nullptr, root->child_at(0)->current_frame_host()->web_ui());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            root->child_at(0)->current_frame_host()->web_ui());
}

// Verify that a WebUI can add a subframe to another WebUI and they will be
// correctly isolated in separate SiteInstances and processes. The subframe
// also uses WebUI with bindings different than the parent to ensure this is
// successfully handled.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUICrossSiteSubframe) {
  GURL main_frame_url(GetWebUIURL("web-ui/page_with_blank_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            root->current_frame_host()->GetEnabledBindings());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Navigate the subframe using renderer-initiated navigation.
  {
    TestFrameNavigationObserver observer(child);
    GURL child_frame_url(
        GetWebUIURL("web-ui-subframe/title2.html?noxfo=true&bindings=" +
                    base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
    EXPECT_TRUE(ExecJs(shell(),
                       JsReplace("document.getElementById($1).src = $2;",
                                 "test_iframe", child_frame_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(child_frame_url, observer.last_committed_url());
    EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
              child->current_frame_host()->GetEnabledBindings());
    EXPECT_EQ(url::Origin::Create(child_frame_url),
              child->current_frame_host()->GetLastCommittedOrigin());
  }
  EXPECT_EQ(GetWebUIURL("web-ui-subframe"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            child->current_frame_host()->web_ui());
  EXPECT_NE(root->current_frame_host()->GetEnabledBindings(),
            child->current_frame_host()->GetEnabledBindings());

  // Navigate once more using renderer-initiated navigation.
  {
    TestFrameNavigationObserver observer(child);
    GURL child_frame_url(
        GetWebUIURL("web-ui-subframe/title3.html?noxfo=true&bindings=" +
                    base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
    EXPECT_TRUE(ExecJs(shell(),
                       JsReplace("document.getElementById($1).src = $2;",
                                 "test_iframe", child_frame_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(child_frame_url, observer.last_committed_url());
    EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
              child->current_frame_host()->GetEnabledBindings());
    EXPECT_EQ(url::Origin::Create(child_frame_url),
              child->current_frame_host()->GetLastCommittedOrigin());
  }

  // Navigate the subframe using browser-initiated navigation.
  {
    TestFrameNavigationObserver observer(child);
    GURL child_frame_url(
        GetWebUIURL("web-ui-subframe/title1.html?noxfo=true&bindings=" +
                    base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
    NavigateFrameToURL(child, child_frame_url);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(child_frame_url, observer.last_committed_url());
    EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
              child->current_frame_host()->GetEnabledBindings());
  }
}

// Verify that SiteInstance and WebUI reuse happens in subframes as well.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIReuseInSubframe) {
  // Disable X-Frame-Options on all WebUIs in this test, since subframe WebUI
  // reuse is expected. If the initial creation does not disable XFO, then
  // subsequent navigations will fail.
  factory()->set_disable_xfo(true);

  GURL main_frame_url(GetWebUIURL("web-ui/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Capture the SiteInstance and WebUI used in the first navigation to compare
  // with the ones used after the reload.
  scoped_refptr<SiteInstance> initial_site_instance =
      child->current_frame_host()->GetSiteInstance();
  WebUI* initial_web_ui = child->current_frame_host()->web_ui();
  GlobalFrameRoutingId initial_rfh_id =
      child->current_frame_host()->GetGlobalFrameRoutingId();

  GURL subframe_same_site_url(GetWebUIURL("web-ui/title2.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_same_site_url);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_same_site_url, observer.last_committed_url());
  }
  EXPECT_EQ(initial_site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, child->current_frame_host()->web_ui());

  // Navigate the child frame cross-site.
  GURL subframe_cross_site_url(GetWebUIURL("web-ui-subframe/title1.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_cross_site_url);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_cross_site_url, observer.last_committed_url());
  }
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            child->current_frame_host()->web_ui());
  EXPECT_NE(initial_web_ui, child->current_frame_host()->web_ui());

  // Capture the new SiteInstance and WebUI of the subframe and navigate it to
  // another document on the same site.
  scoped_refptr<SiteInstance> second_site_instance =
      child->current_frame_host()->GetSiteInstance();
  WebUI* second_web_ui = child->current_frame_host()->web_ui();

  GURL subframe_cross_site_url2(GetWebUIURL("web-ui-subframe/title2.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_cross_site_url2);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_cross_site_url2, observer.last_committed_url());
  }
  EXPECT_EQ(second_site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(second_web_ui, child->current_frame_host()->web_ui());

  // Navigate back to the first document in the subframe, which should bring
  // it back to the initial SiteInstance, but use a different RenderFrameHost
  // and by that a different WebUI instance.
  {
    TestFrameNavigationObserver observer(child);
    shell()->web_contents()->GetController().GoToOffset(-2);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_same_site_url, observer.last_committed_url());
  }
  EXPECT_EQ(initial_site_instance,
            child->current_frame_host()->GetSiteInstance());
  // Use routing id comparison for the RenderFrameHost as the memory allocator
  // sometime places the newly created RenderFrameHost for the back navigation
  // at the same memory location as the initial one. For this reason too, it
  // is not possible to check the web_ui() for inequality, since in some runs
  // the memory in which two different WebUI instances of the same type are
  // placed is the same.
  EXPECT_NE(initial_rfh_id,
            child->current_frame_host()->GetGlobalFrameRoutingId());
}

// Verify that if one WebUI does a window.open() to another WebUI, then the two
// are not sharing a BrowsingInstance, are isolated from each other, and both
// processes have bindings granted to them.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WindowOpenWebUI) {
  GURL test_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(test_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()->web_contents()->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  TestNavigationObserver new_contents_observer(nullptr, 1);
  new_contents_observer.StartWatchingNewWebContents();
  // Execute the script in isolated world since the default CSP disables eval
  // which ExecJs depends on.
  GURL new_tab_url(GetWebUIURL("another-web-ui/title2.html"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1);", new_tab_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  new_contents_observer.Wait();
  EXPECT_TRUE(new_contents_observer.last_navigation_succeeded());

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];

  EXPECT_EQ(new_tab_url, new_shell->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(new_shell->web_contents()->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  // SiteInstances should be different and unrelated due to the
  // BrowsingInstance swaps on navigation.
  EXPECT_NE(new_shell->web_contents()->GetMainFrame()->GetSiteInstance(),
            shell()->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(
      new_shell->web_contents()
          ->GetMainFrame()
          ->GetSiteInstance()
          ->IsRelatedSiteInstance(
              shell()->web_contents()->GetMainFrame()->GetSiteInstance()));

  EXPECT_NE(shell()->web_contents()->GetWebUI(),
            new_shell->web_contents()->GetWebUI());
}

// Test to verify correctness of WebUI and process model in the following
// sequence of navigations:
// * successful navigation to WebUI
// * failed navigation to WebUI
// * failed navigation to http URL
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIFailedNavigation) {
  GURL start_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(start_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  GURL webui_error_url(GetWebUIURL("web-ui/error"));
  EXPECT_FALSE(NavigateToURL(shell(), webui_error_url));
  EXPECT_FALSE(root->current_frame_host()->web_ui());
  EXPECT_EQ(0, root->current_frame_host()->GetEnabledBindings());

  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance()->GetSiteURL(),
              GURL(kUnreachableWebDataURL));
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_error_url(
      embedded_test_server()->GetURL("foo.com", "/nonexistent"));
  EXPECT_FALSE(NavigateToURL(shell(), http_error_url));
  EXPECT_FALSE(root->current_frame_host()->web_ui());
  EXPECT_EQ(0, root->current_frame_host()->GetEnabledBindings());
}

// Verify fetch request to chrome-untrusted:// is blocked.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest,
                       DisallowFetchRequestToChromeUntrusted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL web_url(embedded_test_server()->GetURL("/title2.html"));
  AddUntrustedDataSource(shell()->web_contents()->GetBrowserContext(),
                         "test-host");

  EXPECT_TRUE(NavigateToURL(shell(), web_url));
  EXPECT_EQ(web_url, shell()->web_contents()->GetLastCommittedURL());

  const char kFetchRequestScript[] =
      "(async () => {"
      "  try {"
      "    let response = await fetch($1); "
      "  }"
      "  catch (e) {"
      "    return e.message;"
      "  }"
      "  throw 'Fetch should fail';"
      "})();";
  {
    GURL untrusted_url(GetChromeUntrustedUIURL("test-host/script.js"));
    auto console_delegate = std::make_unique<ConsoleObserverDelegate>(
        shell()->web_contents(),
        "Fetch API cannot load " + untrusted_url.spec() +
            ". URL scheme must be \"http\" or \"https\" for CORS request.");
    shell()->web_contents()->SetDelegate(console_delegate.get());

    EXPECT_EQ("Failed to fetch",
              EvalJs(shell(), JsReplace(kFetchRequestScript, untrusted_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    console_delegate->Wait();
  }
}

// Verify XHR request to chrome-untrusted:// is blocked.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, DisallowXHRRequestToChromeUntrusted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL web_url(embedded_test_server()->GetURL("/title2.html"));
  AddUntrustedDataSource(shell()->web_contents()->GetBrowserContext(),
                         "test-host");

  EXPECT_TRUE(NavigateToURL(shell(), web_url));
  EXPECT_EQ(web_url, shell()->web_contents()->GetLastCommittedURL());

  const char kXHRRequest[] =
      "new Promise((resolve) => {"
      "  const xhttp = new XMLHttpRequest();"
      "  xhttp.open('GET', $1, true);"
      "  xhttp.onload = () => { "
      "    resolve('Request should have failed');"
      "  };"
      "  xhttp.onerror = () => {"
      "    resolve('Request failed');"
      "  };"
      "  xhttp.send();"
      "}); ";
  {
    GURL untrusted_url(GetChromeUntrustedUIURL("test-host/script.js"));
    const std::string host = web_url.GetOrigin().spec();

    auto console_delegate = std::make_unique<ConsoleObserverDelegate>(
        shell()->web_contents(),
        "Access to XMLHttpRequest at '" + untrusted_url.spec() +
            "' from origin '" + host.substr(0, host.length() - 1) +
            "' has been blocked by CORS policy: Cross origin requests are only "
            "supported for protocol schemes: http, data, chrome, https.");

    shell()->web_contents()->SetDelegate(console_delegate.get());
    EXPECT_EQ("Request failed",
              EvalJs(shell(), JsReplace(kXHRRequest, untrusted_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    console_delegate->Wait();
  }
}

// Verify load script from chrome-untrusted:// is blocked.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest,
                       DisallowResourceRequestToChromeUntrusted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL web_url(embedded_test_server()->GetURL("/title2.html"));
  AddUntrustedDataSource(shell()->web_contents()->GetBrowserContext(),
                         "test-host");

  EXPECT_TRUE(NavigateToURL(shell(), web_url));
  EXPECT_EQ(web_url, shell()->web_contents()->GetLastCommittedURL());

  const char kLoadResourceScript[] =
      "new Promise((resolve) => {"
      "  const script = document.createElement('script');"
      "  script.onload = () => {"
      "    resolve('Script load should have failed');"
      "  };"
      "  script.onerror = () => {"
      "    resolve('Load failed');"
      "  };"
      "  script.src = $1;"
      "  document.body.appendChild(script);"
      "});";

  // There are no error messages in the console which is why we cannot check for
  // them.
  {
    GURL untrusted_url(GetChromeUntrustedUIURL("test-host/script.js"));
    EXPECT_EQ("Load failed",
              EvalJs(shell(), JsReplace(kLoadResourceScript, untrusted_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  }
}

}  // namespace content
