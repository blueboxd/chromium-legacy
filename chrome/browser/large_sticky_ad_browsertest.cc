// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

namespace {

enum class StickyType {
  kReposition,
  kFixedPosition,
  kStickyPosition,
  kNotSticky,
};

base::StringPiece StickyTypeToString(StickyType type) {
  switch (type) {
    case StickyType::kReposition:
      return "reposition";
    case StickyType::kFixedPosition:
      return "fixed_position";
    case StickyType::kStickyPosition:
      return "sticky_position";
    case StickyType::kNotSticky:
      return "not_sticky";
  }
}

}  // namespace

class LargeStickyAdBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  ~LargeStickyAdBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("title1.html")});
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void ExecuteScriptOnFrame(const std::string& script,
                            content::RenderFrameHost* rfh) {
    rfh->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script),
                                   base::NullCallback());
  }

  content::RenderFrameHost* AppendChildToFrame(const std::string& frame_id,
                                               const GURL& subframe_url,
                                               StickyType sticky_type,
                                               double frame_height_ratio,
                                               content::RenderFrameHost* rfh) {
    content::TestNavigationObserver navigation_observer(web_contents());
    std::string sticky_type_str = StickyTypeToString(sticky_type).data();
    ExecuteScriptOnFrame(
        base::StringPrintf("appendFrame('%s','%s','%s','%f')", frame_id.c_str(),
                           subframe_url.spec().c_str(), sticky_type_str.c_str(),
                           frame_height_ratio),
        rfh);
    navigation_observer.Wait();

    return content::FrameMatchingPredicate(
        web_contents(),
        base::BindRepeating(&content::FrameMatchesName, frame_id));
  }
};

class LargeStickyAdBrowserTestStickyChildAd
    : public LargeStickyAdBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<StickyType, bool /* remote_subframe */>> {};

// This test suite tests sticky child frame achieved by re-position, fixed
// position, and sticky position. The test is also parameterized by whether the
// sticky frame is local or remote to the top frame.
IN_PROC_BROWSER_TEST_P(LargeStickyAdBrowserTestStickyChildAd,
                       StickyAdDetected) {
  bool remote_subframe;
  StickyType sticky_type;
  std::tie(sticky_type, remote_subframe) = GetParam();

  page_load_metrics::PageLoadMetricsTestWaiter web_feature_waiter(
      web_contents());

  std::string host1 = "foo.com";
  std::string host2 = remote_subframe ? "bar.com" : host1;

  GURL top_frame_url =
      embedded_test_server()->GetURL(host1, "/sticky_frame_test_factory.html");
  ui_test_utils::NavigateToURL(browser(), top_frame_url);

  GURL subframe_url = embedded_test_server()->GetURL(host2, "/title1.html");

  ExecuteScriptOnFrame("appendSmallParagraph()",
                       web_contents()->GetMainFrame());
  AppendChildToFrame("frame_id", subframe_url, sticky_type, 0.5,
                     web_contents()->GetMainFrame());
  ExecuteScriptOnFrame("appendLargeParagraph()",
                       web_contents()->GetMainFrame());
  ExecuteScriptOnFrame("autoScrollUp()", web_contents()->GetMainFrame());

  web_feature_waiter.AddWebFeatureExpectation(
      blink::mojom::WebFeature::kLargeStickyAd);

  web_feature_waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LargeStickyAdBrowserTestStickyChildAd,
    ::testing::Combine(::testing::Values(StickyType::kReposition,
                                         StickyType::kFixedPosition,
                                         StickyType::kStickyPosition),
                       ::testing::Bool()));

class LargeStickyAdBrowserTestFixedChildDefaultGrandchildAd
    : public LargeStickyAdBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* remote_subframe */,
                     bool /* remote_grandchild */>> {};

// Test a A(B(C)) setup where B is a large fixed-positioned non-ad frame, and C
// is an large default-positioned ad frame. Content in A keeps scrolling. C
// should be determined to be a large sticky ad.
//
// When A and B are remote in particular, this tests the scenario where when
// page A is scrolling, there's no animation frame in B, and normally no
// rendering / intersection-observation will be scheduled in B. However, the
// sticky frame tracking logic should force the rendering in B so that frame C
// can be continuously tracked while scrolling.
IN_PROC_BROWSER_TEST_P(LargeStickyAdBrowserTestFixedChildDefaultGrandchildAd,
                       StickyAdDetected) {
  bool remote_subframe;
  bool remote_grandchild;
  std::tie(remote_subframe, remote_grandchild) = GetParam();

  if (remote_subframe &&
      !base::FeatureList::IsEnabled(
          blink::features::kForceExtraRenderingToTrackStickyFrame)) {
    return;
  }

  std::string host1 = "foo.com";
  std::string host2 = remote_subframe ? "bar.com" : host1;
  std::string host3 = remote_grandchild ? "baz.com" : host2;

  page_load_metrics::PageLoadMetricsTestWaiter web_feature_waiter(
      web_contents());

  GURL top_frame_url =
      embedded_test_server()->GetURL(host1, "/sticky_frame_test_factory.html");
  ui_test_utils::NavigateToURL(browser(), top_frame_url);

  GURL subframe_url =
      embedded_test_server()->GetURL(host2, "/sticky_frame_test_factory.html");
  GURL grandchild_url = embedded_test_server()->GetURL(host3, "/title1.html");

  ExecuteScriptOnFrame("appendSmallParagraph()",
                       web_contents()->GetMainFrame());
  content::RenderFrameHost* child_1 =
      AppendChildToFrame("frame_1", subframe_url, StickyType::kFixedPosition,
                         0.6, web_contents()->GetMainFrame());
  AppendChildToFrame("frame_2", grandchild_url, StickyType::kNotSticky, 0.6,
                     child_1);
  ExecuteScriptOnFrame("appendLargeParagraph()",
                       web_contents()->GetMainFrame());
  ExecuteScriptOnFrame("autoScrollUp()", web_contents()->GetMainFrame());

  web_feature_waiter.AddWebFeatureExpectation(
      blink::mojom::WebFeature::kLargeStickyAd);

  web_feature_waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LargeStickyAdBrowserTestFixedChildDefaultGrandchildAd,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));

class LargeStickyAdBrowserTestScrollingTopPageReverseMovingGrandchildAd
    : public LargeStickyAdBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* remote_subframe */,
                     bool /* remote_grandchild */>> {};

// Test a A(B(C)) setup. B is being scrolled up as the top page scrolls, and C
// is being positioned downward relative to B at the same speed. The total
// effect is that C is sticky to the browser viewport. The test is parameterized
// by whether the B is local or remote to A and whether C is local to B (or
// remote to both A and B).

// Flaky timeout on most/all platforms: crbug.com/1049073
IN_PROC_BROWSER_TEST_P(
    LargeStickyAdBrowserTestScrollingTopPageReverseMovingGrandchildAd,
    DISABLED_StickyAdDetected) {
  bool remote_subframe;
  bool remote_grandchild;
  std::tie(remote_subframe, remote_grandchild) = GetParam();

  page_load_metrics::PageLoadMetricsTestWaiter web_feature_waiter(
      web_contents());

  std::string host1 = "foo.com";
  std::string host2 = remote_subframe ? "bar.com" : host1;
  std::string host3 = remote_grandchild ? "baz.com" : host2;

  GURL top_frame_url =
      embedded_test_server()->GetURL(host1, "/sticky_frame_test_factory.html");
  GURL subframe_url =
      embedded_test_server()->GetURL(host2, "/sticky_frame_test_factory.html");
  GURL grandchild_frame_url =
      embedded_test_server()->GetURL(host3, "/title1.html");

  ui_test_utils::NavigateToURL(browser(), top_frame_url);

  ExecuteScriptOnFrame("appendSmallParagraph()",
                       web_contents()->GetMainFrame());
  content::RenderFrameHost* child1 =
      AppendChildToFrame("child1", subframe_url, StickyType::kNotSticky, 0.8,
                         web_contents()->GetMainFrame());
  AppendChildToFrame("child2", grandchild_frame_url, StickyType::kFixedPosition,
                     0.5, child1);
  ExecuteScriptOnFrame("appendLargeParagraph()",
                       web_contents()->GetMainFrame());

  ExecuteScriptOnFrame("autoMoveFirstIframeDown()", child1);
  ExecuteScriptOnFrame("autoScrollUp()", web_contents()->GetMainFrame());

  web_feature_waiter.AddWebFeatureExpectation(
      blink::mojom::WebFeature::kLargeStickyAd);

  web_feature_waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LargeStickyAdBrowserTestScrollingTopPageReverseMovingGrandchildAd,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));
