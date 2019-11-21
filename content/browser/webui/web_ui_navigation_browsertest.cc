// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_browsertest_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"

namespace content {

class WebUINavigationBrowserTest : public ContentBrowserTest {
 public:
  WebUINavigationBrowserTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }
  ~WebUINavigationBrowserTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Verify that no web content can be loaded in a process that has WebUI
  // bindings, regardless of what scheme the content was loaded from.
  void TestWebFrameInWebUIProcessDisallowed(int bindings) {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    GURL data_url("data:text/html,a data url document");
    EXPECT_TRUE(NavigateToURL(shell(), data_url));
    EXPECT_EQ(data_url, root->current_frame_host()->GetLastCommittedURL());
    EXPECT_FALSE(
        ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
            root->current_frame_host()->GetProcess()->GetID()));

    // Grant WebUI bindings to the process. This will ensure that if there is
    // a mistake in the navigation logic and a process gets somehow WebUI
    // bindings, it cannot include web content regardless of the scheme of the
    // document.
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        root->current_frame_host()->GetProcess()->GetID(), bindings);
    EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
        root->current_frame_host()->GetProcess()->GetID()));
    {
      GURL web_url(embedded_test_server()->GetURL("/title2.html"));
      std::string script = base::StringPrintf(
          "var frame = document.createElement('iframe');\n"
          "frame.src = '%s';\n"
          "document.body.appendChild(frame);\n",
          web_url.spec().c_str());

      TestNavigationObserver navigation_observer(shell()->web_contents());
      EXPECT_TRUE(ExecuteScript(shell(), script));
      navigation_observer.Wait();

      EXPECT_EQ(1U, root->child_count());
      EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
    }
  }

  // Verify that a WebUI document in a subframe is allowed to target a new
  // window and navigate it to web content.
  void TestWebUISubframeNewWindowToWebAllowed(int bindings) {
    GURL main_frame_url(
        GetWebUIURL("web-ui/page_with_blank_iframe.html?bindings=" +
                    base::NumberToString(bindings)));
    EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    EXPECT_EQ(1U, root->child_count());
    FrameTreeNode* child = root->child_at(0);

    EXPECT_EQ(bindings, root->current_frame_host()->GetEnabledBindings());
    EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    RenderFrameHost* webui_rfh = root->current_frame_host();
    EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
        webui_rfh->GetProcess()->GetID()));

    // Navigate the subframe to the same WebUI.
    {
      TestFrameNavigationObserver observer(root->child_at(0));
      GURL subframe_url(GetWebUIURL("web-ui/title1.html?noxfo=true&bindings=" +
                                    base::NumberToString(bindings)));
      NavigateFrameToURL(root->child_at(0), subframe_url);

      EXPECT_TRUE(observer.last_navigation_succeeded());
      EXPECT_EQ(subframe_url, observer.last_committed_url());
    }

    // Add a link that targets a new window and click it.
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    std::string script = JsReplace(
        "var a = document.createElement('a');"
        "a.href = $1; a.target = '_blank'; a.click()",
        web_url.spec().c_str());

    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(root->child_at(0)->current_frame_host(), script,
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    Shell* new_shell = new_shell_observer.GetShell();
    WebContents* new_web_contents = new_shell->web_contents();
    WaitForLoadStop(new_web_contents);

    EXPECT_EQ(web_url, new_web_contents->GetLastCommittedURL());

    FrameTreeNode* new_root =
        static_cast<WebContentsImpl*>(new_web_contents)->GetFrameTree()->root();
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              new_root->current_frame_host()->GetSiteInstance());
    EXPECT_NE(root->current_frame_host()->GetProcess(),
              new_root->current_frame_host()->GetProcess());
    EXPECT_NE(root->current_frame_host()->web_ui(),
              new_root->current_frame_host()->web_ui());
    EXPECT_NE(root->current_frame_host()->GetEnabledBindings(),
              new_root->current_frame_host()->GetEnabledBindings());
    EXPECT_FALSE(
        root->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
            new_root->current_frame_host()->GetSiteInstance()));
  }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUINavigationBrowserTest);
};

// Verify that a chrome: scheme document cannot add iframes with web content.
// See https://crbug.com/683418.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeSchemeDisallowed) {
  // Serve a WebUI with no iframe restrictions.
  GURL main_frame_url(GetWebUIURL("web-ui/title1.html?noxfo=true&childsrc="));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            root->current_frame_host()->GetEnabledBindings());

  std::string add_iframe_script =
      "var frame = document.createElement('iframe');\n"
      "frame.src = $1;\n"
      "document.body.appendChild(frame);\n";

  // Navigate to a Web URL and verify that the navigation was blocked.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    EXPECT_TRUE(ExecJs(shell(), JsReplace(add_iframe_script, web_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // Navigate to a data URL and verify that the navigation was blocked.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL data_url("data:text/html,foo");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(add_iframe_script, data_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // Verify that an iframe with "about:blank" URL is actually allowed. Not
  // sure why this would be useful, but from a security perspective it can
  // only host content coming from the parent document, so it effectively
  // has the same security context.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL about_blank_url("about:blank");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(add_iframe_script, about_blank_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// Verify that a chrome: scheme document cannot add iframes with web content
// and does not crash if the navigation is blocked by CSP.
// See https://crbug.com/944086.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeSchemeDisallowedByCSP) {
  // Use a WebUI with restrictive CSP that disallows subframes. This will cause
  // the navigation to fail due to the CSP check and ensure this behaves the
  // same way as the repro steps in https://crbug.com/944086.
  GURL main_frame_url(
      GetWebUIURL("web-ui/title1.html?childsrc=child-src 'none'"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_EQ(main_frame_url, shell()->web_contents()->GetLastCommittedURL());

  {
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(shell(),
               JsReplace("var frame = document.createElement('iframe');\n"
                         "frame.src = $1;\n"
                         "document.body.appendChild(frame);\n",
                         web_url),
               EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    navigation_observer.Wait();

    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }
}

// Verify that a WebUI document in the main frame is allowed to navigate to
// web content and it properly does cross-process navigation.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest, WebUIMainFrameToWebAllowed) {
  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), chrome_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderFrameHost* webui_rfh = root->current_frame_host();
  scoped_refptr<SiteInstance> webui_site_instance =
      webui_rfh->GetSiteInstance();

  EXPECT_EQ(chrome_url, webui_rfh->GetLastCommittedURL());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      webui_rfh->GetProcess()->GetID()));
  EXPECT_EQ(ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                root->current_frame_host()->GetProcess()->GetID()),
            webui_site_instance->GetSiteURL());

  GURL web_url(embedded_test_server()->GetURL("/title2.html"));
  std::string script =
      base::StringPrintf("location.href = '%s';", web_url.spec().c_str());

  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell(), script));
  navigation_observer.Wait();

  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(web_url, root->current_frame_host()->GetLastCommittedURL());
  EXPECT_NE(webui_site_instance, root->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(webui_site_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      root->current_frame_host()->GetProcess()->GetID()));
  EXPECT_NE(ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                root->current_frame_host()->GetProcess()->GetID()),
            webui_site_instance->GetSiteURL());
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInWebUIProcessDisallowed) {
  TestWebFrameInWebUIProcessDisallowed(BINDINGS_POLICY_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInMojoWebUIProcessDisallowed) {
  TestWebFrameInWebUIProcessDisallowed(BINDINGS_POLICY_MOJO_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInHybridWebUIProcessDisallowed) {
  TestWebFrameInWebUIProcessDisallowed(BINDINGS_POLICY_MOJO_WEB_UI |
                                       BINDINGS_POLICY_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(BINDINGS_POLICY_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       MojoWebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(BINDINGS_POLICY_MOJO_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       HybridWebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(BINDINGS_POLICY_MOJO_WEB_UI |
                                         BINDINGS_POLICY_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebUIOriginsRequireDedicatedProcess) {
  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  GURL expected_site_url(GetWebUIURL("web-ui"));

  // chrome:// URLs should require a dedicated process.
  WebContents* web_contents = shell()->web_contents();
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      IsolationContext(browser_context), chrome_url));

  // Navigate to a WebUI page.
  EXPECT_TRUE(NavigateToURL(shell(), chrome_url));

  // Verify that the "hostname" is also part of the site URL.
  GURL site_url = web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL();
  EXPECT_EQ(expected_site_url, site_url);

  // Ask the page to create a blob URL and return back the blob URL.
  const char* kScript = R"(
          var blob = new Blob(['foo'], {type : 'text/html'});
          var url = URL.createObjectURL(blob);
          url;
      )";
  GURL blob_url(
      EvalJs(shell(), kScript, EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */)
          .ExtractString());
  EXPECT_EQ(url::kBlobScheme, blob_url.scheme());

  // Verify that the blob also requires a dedicated process and that it would
  // use the same site url as the original page.
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      IsolationContext(browser_context), blob_url));
  EXPECT_EQ(expected_site_url,
            SiteInstance::GetSiteForURL(browser_context, blob_url));
}

// Verify that navigating back/forward between WebUI and an error page for a
// failed WebUI navigation works correctly.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       SessionHistoryToFailedNavigation) {
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
  EXPECT_EQ(0 /* no bindings */,
            root->current_frame_host()->GetEnabledBindings());

  GURL success_url(GetWebUIURL("web-ui/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), success_url));
  EXPECT_EQ(success_url, shell()->web_contents()->GetLastCommittedURL());

  {
    TestFrameNavigationObserver observer(root);
    shell()->web_contents()->GetController().GoBack();
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_FALSE(root->current_frame_host()->web_ui());
  }

  {
    TestFrameNavigationObserver observer(root);
    shell()->web_contents()->GetController().GoForward();
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_TRUE(root->current_frame_host()->web_ui());
    EXPECT_EQ(success_url, observer.last_committed_url());
  }
}

}  // namespace content
