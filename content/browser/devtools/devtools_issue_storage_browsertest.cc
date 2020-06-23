// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class DevToolsIssueStorageBrowserTest : public DevToolsProtocolTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
  }
};

namespace {

void ReportDummyIssue(RenderFrameHostImpl* rfh) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
  auto inspector_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue)
          .SetDetails(issueDetails.Build())
          .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfh,
                                                        inspector_issue.get());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssues) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Report an empty SameSite cookie issue.
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* root = web_contents_impl->GetFrameTree()->GetMainFrame();
  ReportDummyIssue(root);

  // 3) Open DevTools.
  Attach();

  // 4) Verify we haven't received any Issues yet.
  ASSERT_TRUE(notifications_.empty());

  // 5) Enable audits domain.
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 6) Verify we have received the SameSite issue.
  WaitForNotification("Audits.issueAdded", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssuesWhileAttached) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Open DevTools and enable Audits domain.
  Attach();
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 3) Verify we haven't received any Issues yet.
  ASSERT_TRUE(notifications_.empty());

  // 4) Report an empty SameSite cookie issue.
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* root = web_contents_impl->GetFrameTree()->GetMainFrame();
  ReportDummyIssue(root);

  // 5) Verify we have received the SameSite issue.
  WaitForNotification("Audits.issueAdded", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DeleteSubframeWithIssue) {
  // 1) Navigate to a page with an OOP iframe.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      embedded_test_server()->GetURL("/devtools/page-with-oopif.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // 2) Report an empty SameSite cookie issue in the iframe.
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* root = web_contents_impl->GetFrameTree()->GetMainFrame();
  EXPECT_EQ(root->child_count(), static_cast<unsigned>(1));
  RenderFrameHostImpl* iframe = root->child_at(0)->current_frame_host();
  EXPECT_FALSE(iframe->is_main_frame());

  ReportDummyIssue(iframe);

  // 3) Delete the iframe from the page. This should cause the issue to be
  // re-assigned
  //    to the root frame.
  root->RemoveChild(iframe->frame_tree_node());

  // 4) Open DevTools and enable Audits domain.
  Attach();
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 5) Verify we have received the SameSite issue on the main target.
  WaitForNotification("Audits.issueAdded", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       MainFrameNavigationClearsIssues) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Report an empty SameSite cookie issue.
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* root = web_contents_impl->GetFrameTree()->GetMainFrame();
  ReportDummyIssue(root);

  // 3) Navigate to /devtools/navigation.html
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // 4) Open DevTools and enable Audits domain.
  Attach();
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 5) Verify that we haven't received any notifications.
  ASSERT_TRUE(notifications_.empty());
}

}  // namespace content
