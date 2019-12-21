// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

namespace content {

class CrossOriginOpenerPolicyBrowserTest : public ContentBrowserTest {
 public:
  CrossOriginOpenerPolicyBrowserTest() {
    std::vector<base::Feature> features;
    feature_list_.InitWithFeatures({network::features::kCrossOriginIsolation},
                                   {});
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOrigin) {
  GURL starting_page(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->SetCrossOriginOriginOpenerPolicyForTesting(
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->SetCrossOriginOriginOpenerPolicyForTesting(
      network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInherit) {
  GURL starting_page(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->SetCrossOriginOriginOpenerPolicyForTesting(
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kUnsafeNone);
}

}  // namespace content
