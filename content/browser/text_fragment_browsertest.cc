// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class TextFragmentAnchorBrowserTest : public ContentBrowserTest {
 public:
  TextFragmentAnchorBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kDocumentPolicy);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
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

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest, EnabledOnUserNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/target_text_link.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContents* main_contents = shell()->web_contents();
  TestNavigationObserver observer(main_contents);
  RenderFrameSubmissionObserver frame_observer(main_contents);

  // We need to wait until hit test data is available.
  HitTestRegionObserver hittest_observer(GetWidgetHost()->GetFrameSinkId());
  hittest_observer.WaitForHitTestData();

  ClickElementWithId(main_contents, "link");
  observer.Wait();
  EXPECT_EQ(target_text_url, main_contents->GetLastCommittedURL());

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(
      /*expected_scroll_offset_at_top=*/false);
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(true, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       EnabledOnBrowserNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(
      /*expected_scroll_offset_at_top=*/false);
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(true, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       EnabledOnUserGestureScriptNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));

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
  frame_observer.WaitForScrollOffsetAtTop(
      /*expected_scroll_offset_at_top=*/false);
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(true, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       DisabledOnScriptNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));

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
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(false, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       DisabledOnScriptHistoryNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));
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
  RunUntilInputProcessed(GetWidgetHost());

  // Note: we use a scroll handler in the page to check whether any scrolls
  // happened at all, rather than checking the current scroll offset. This is
  // to ensure that if the offset is reset back to the top for other reasons
  // (e.g. history restoration) we still fail this test. See
  // https://crbug.com/1042986 for why this matters.
  EXPECT_EQ(false, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       EnabledOnSameDocumentBrowserNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(false);

  // Scroll the page back to top. Make sure we reset the |did_scroll| variable
  // we'll use below to ensure the same-document navigation invokes the text
  // fragment.
  EXPECT_TRUE(ExecuteScript(main_contents, "window.scrollTo(0, 0)"));
  frame_observer.WaitForScrollOffsetAtTop(true);
  EXPECT_TRUE(ExecJs(main_contents, "did_scroll = false;"));

  // Perform a same-document browser initiated navigation
  GURL same_doc_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=some"));
  EXPECT_TRUE(NavigateToURL(shell(), same_doc_url));

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(
      /*expected_scroll_offset_at_top=*/false);
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(true, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       DisabledOnSameDocumentScriptNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/scrollable_page_with_content.html"));
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=some"));

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
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(false, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest, EnabledByDocumentPolicy) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/target.html");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/target.html#:~:text=text"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  // Load the target document
  TestNavigationManager navigation_manager(main_contents, url);
  shell()->LoadURL(url);

  // Start navigation
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // Send Document-Policy header
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Document-Policy: no-force-load-at-top\r\n"
      "\r\n"
      "<script>"
      "  let did_scroll = false;"
      "  window.addEventListener('scroll', () => {"
      "    did_scroll = true;"
      "  });"
      "</script>"
      "<p style='position: absolute; top: 10000px;'>Some text</p>");
  response.Done();

  EXPECT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();
  navigation_manager.WaitForNavigationFinished();

  WaitForPageLoad(main_contents);
  frame_observer.WaitForScrollOffsetAtTop(
      /*expected_scroll_offset_at_top=*/false);
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(true, EvalJs(main_contents, "did_scroll;"));
}

IN_PROC_BROWSER_TEST_F(TextFragmentAnchorBrowserTest,
                       DisabledByDocumentPolicy) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/target.html");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/target.html#:~:text=text"));
  WebContents* main_contents = shell()->web_contents();

  // Load the target document
  TestNavigationManager navigation_manager(main_contents, url);
  shell()->LoadURL(url);

  // Start navigation
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // Send Document-Policy header
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Document-Policy: force-load-at-top\r\n"
      "\r\n"
      "<script>"
      "  let did_scroll = false;"
      "  window.addEventListener('scroll', () => {"
      "    did_scroll = true;"
      "  });"
      "</script>"
      "<p style='position: absolute; top: 10000px;'>Some text</p>");
  response.Done();

  EXPECT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();
  navigation_manager.WaitForNavigationFinished();

  WaitForPageLoad(main_contents);
  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  RunUntilInputProcessed(GetWidgetHost());
  EXPECT_EQ(false, EvalJs(main_contents, "did_scroll;"));
}

class ForceLoadAtTopBrowserTest : public TextFragmentAnchorBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    TextFragmentAnchorBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TextFragmentAnchorBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ForceLoadAtTop");
  }
};

// Test that scroll restoration is disabled with ForceLoadAtTop
IN_PROC_BROWSER_TEST_F(ForceLoadAtTopBrowserTest, ScrollRestorationDisabled) {
  GURL url(
      embedded_test_server()->GetURL("/scrollable_page_with_content.html"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(WaitForRenderFrameReady(main_contents->GetMainFrame()));

  // Scroll down the page a bit
  EXPECT_TRUE(ExecuteScript(main_contents, "window.scrollTo(0, 1000)"));
  frame_observer.WaitForScrollOffsetAtTop(false);

  // Navigate away
  EXPECT_TRUE(ExecuteScript(main_contents, "window.location = 'about:blank'"));
  EXPECT_TRUE(WaitForLoadStop(main_contents));
  EXPECT_TRUE(WaitForRenderFrameReady(main_contents->GetMainFrame()));

  // Navigate back
  EXPECT_TRUE(ExecuteScript(main_contents, "history.back()"));
  EXPECT_TRUE(WaitForLoadStop(main_contents));
  EXPECT_TRUE(WaitForRenderFrameReady(main_contents->GetMainFrame()));

  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  RunUntilInputProcessed(RenderWidgetHostImpl::From(
      main_contents->GetRenderViewHost()->GetWidget()));
  EXPECT_TRUE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

// Test that element fragment anchor scrolling is disabled with ForceLoadAtTop
IN_PROC_BROWSER_TEST_F(ForceLoadAtTopBrowserTest, FragmentAnchorDisabled) {
  GURL url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#text"));
  WebContents* main_contents = shell()->web_contents();

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(WaitForRenderFrameReady(main_contents->GetMainFrame()));

  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  RunUntilInputProcessed(RenderWidgetHostImpl::From(
      main_contents->GetRenderViewHost()->GetWidget()));
  EXPECT_TRUE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

IN_PROC_BROWSER_TEST_F(ForceLoadAtTopBrowserTest, TextFragmentAnchorDisabled) {
  GURL url(embedded_test_server()->GetURL(
      "/scrollable_page_with_content.html#:~:text=text"));
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(WaitForRenderFrameReady(main_contents->GetMainFrame()));

  // Wait a short amount of time to ensure the page does not scroll.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  RunUntilInputProcessed(RenderWidgetHostImpl::From(
      main_contents->GetRenderViewHost()->GetWidget()));
  EXPECT_TRUE(main_contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

}  // namespace content
