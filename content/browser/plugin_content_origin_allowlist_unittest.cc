// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_content_origin_allowlist.h"

#include <memory>

#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

class PluginContentOriginAllowlistTest : public RenderViewHostTestHarness {
 public:
  PluginContentOriginAllowlistTest() = default;
  ~PluginContentOriginAllowlistTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginContentOriginAllowlistTest);
};

TEST_F(PluginContentOriginAllowlistTest, ClearAllowlistOnNavigate) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());

  // 1) Navigate to A.
  GURL url_a(https_server.GetURL("a.com", "/title1.html"));
  RenderFrameHost* rfh_a =
      NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url_a);

  // 2) Allowlist an origin on Page A.
  url::Origin allow_origin = url::Origin::Create(GURL("http://www.google.com"));
  static_cast<WebContentsImpl*>(web_contents())
      ->plugin_content_origin_allowlist_->OnPluginContentOriginAllowed(
          rfh_a, allow_origin);
  EXPECT_TRUE(
      PluginContentOriginAllowlist::IsOriginAllowlistedForFrameForTesting(
          rfh_a, allow_origin));

  // 3) Navigate to B and confirm that the allowlist is cleared.
  GURL url_b(https_server.GetURL("b.com", "/title2.html"));
  RenderFrameHost* rfh_b =
      NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url_b);
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_FALSE(
      PluginContentOriginAllowlist::IsOriginAllowlistedForFrameForTesting(
          rfh_b, allow_origin));
}

TEST_F(PluginContentOriginAllowlistTest, SubframeInheritsAllowlist) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());

  // 1) Navigate to A.
  GURL url_a(https_server.GetURL("a.com", "/title1.html"));
  RenderFrameHost* rfh_a =
      NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url_a);

  // 2) Allowlist an origin on Page A.
  url::Origin allow_origin = url::Origin::Create(GURL("http://www.google.com"));
  static_cast<WebContentsImpl*>(web_contents())
      ->plugin_content_origin_allowlist_->OnPluginContentOriginAllowed(
          rfh_a, allow_origin);
  EXPECT_TRUE(
      PluginContentOriginAllowlist::IsOriginAllowlistedForFrameForTesting(
          rfh_a, allow_origin));

  // 3) Create a frame inside Page A, and confirm that the allowlist is passed
  // on.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(rfh_a);
  RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  EXPECT_NE(rfh_a, subframe);
  EXPECT_TRUE(
      PluginContentOriginAllowlist::IsOriginAllowlistedForFrameForTesting(
          subframe, allow_origin));
}

}  // namespace content
