// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/unguessable_token.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class EmbeddingTokenBrowserTest : public ContentBrowserTest {
 public:
  EmbeddingTokenBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  RenderFrameHostImpl* top_frame_host() {
    return static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame());
  }

  EmbeddingTokenBrowserTest(const EmbeddingTokenBrowserTest&) = delete;
  EmbeddingTokenBrowserTest& operator=(const EmbeddingTokenBrowserTest&) =
      delete;

 private:
  FrameTreeVisualizer visualizer_;
};

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest, NoEmbeddingTokenOnMainFrame) {
  GURL a_url = embedded_test_server()->GetURL("a.com", "/site_isolation/");
  GURL b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  // Starts without an embedding token.
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Embedding tokens don't get added to the main frame.
  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html")));
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  EXPECT_TRUE(NavigateToURL(shell(), b_url.Resolve("blank.html")));
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokensAddedToCrossSiteIFrames) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b(a),c,a)")));

  ASSERT_EQ(3U, top_frame_host()->child_count());
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Child 0 (b) should have an embedding token.
  auto child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);

  // Child 0 (a) of Child 0 (b) should have an embedding token.
  ASSERT_EQ(1U, top_frame_host()->child_at(0)->child_count());
  auto child_0_0_token = top_frame_host()
                             ->child_at(0)
                             ->child_at(0)
                             ->current_frame_host()
                             ->GetEmbeddingToken();
  ASSERT_TRUE(child_0_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_0_token);
  EXPECT_NE(child_0_token, child_0_0_token);

  // Child 1 (c) should have an embedding token.
  auto child_1_token =
      top_frame_host()->child_at(1)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_1_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_1_token);
  EXPECT_NE(child_0_token, child_1_token);
  EXPECT_NE(child_0_0_token, child_1_token);

  // Child 2 (a) shouldn't have an embedding token as it is same site.
  EXPECT_FALSE(top_frame_host()
                   ->child_at(2)
                   ->current_frame_host()
                   ->GetEmbeddingToken()
                   .has_value());

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokensSwapOnCrossSiteNavigations) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Child 0 (b) should have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);

  // Navigate child 0 (b) to another site (cross-process) the token should swap.
  {
    RenderFrameDeletedObserver deleted_observer(target);
    NavigateIframeToURL(shell()->web_contents(), "child-0",
                        embedded_test_server()
                            ->GetURL("c.com", "/site_isolation/")
                            .Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }
  auto new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(new_child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), new_child_0_token);
  EXPECT_NE(child_0_token, new_child_0_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokensDoNotSwapOnSameSiteNavigations) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());
  auto b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  // Navigate child 0 to another site (cross-process) a token should be created.
  {
    RenderFrameDeletedObserver deleted_observer(
        top_frame_host()->child_at(0)->current_frame_host());
    NavigateIframeToURL(web_contents(), "child-0", b_url.Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }

  // Child 0 (b) should have an embedding token.
  auto child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);

  // Navigate child 0 (b) to same origin the token should not swap.
  NavigateIframeToURL(web_contents(), "child-0", b_url.Resolve("valid.html"));
  auto new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(new_child_0_token.has_value());
  // If we are creating a new frame even for same-site navigations then we would
  // expect a different token.
  if (CreateNewHostForSameSiteSubframe()) {
    EXPECT_NE(child_0_token, new_child_0_token);
  } else {
    EXPECT_EQ(child_0_token, new_child_0_token);
  }

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokenRemovedOnSubframeNavigationToSameOrigin) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/");
  EXPECT_TRUE(NavigateToURL(
      shell(), a_url.Resolve("cross_site_iframe_factory.html?a(b)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Child 0 (b) should have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);

  // Navigate child 0 (b) to the same site as the main frame. This shouldn't
  // create an embedding token as the child is now local.
  {
    RenderFrameDeletedObserver deleted_observer(target);
    NavigateIframeToURL(web_contents(), "child-0",
                        a_url.Resolve("site_isolation/").Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }
  ASSERT_FALSE(top_frame_host()
                   ->child_at(0)
                   ->current_frame_host()
                   ->GetEmbeddingToken()
                   .has_value());

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(
    EmbeddingTokenBrowserTest,
    EmbeddingTokenAddedOnSubframeNavigationAwayFromSameOrigin) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/");
  EXPECT_TRUE(NavigateToURL(
      shell(), a_url.Resolve("cross_site_iframe_factory.html?a(a)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Child shouldn't have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  EXPECT_FALSE(child_0_token.has_value());

  // Navigate child 0 to a different site. This should now have an embedding
  // token.
  {
    RenderFrameDeletedObserver deleted_observer(target);
    NavigateIframeToURL(web_contents(), "child-0",
                        embedded_test_server()
                            ->GetURL("b.com", "/site_isolation/")
                            .Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }
  auto new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(new_child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), new_child_0_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

}  // namespace content
