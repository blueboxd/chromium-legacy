// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_predictor.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
const char kTestUrl[] = "https://www.test.com/";
}

class TestPreviewsLitePagePredictor : public PreviewsLitePagePredictor {
 public:
  TestPreviewsLitePagePredictor(content::WebContents* web_contents,
                                bool data_saver_enabled,
                                bool ect_is_slow,
                                bool page_is_blacklisted,
                                bool is_visible)
      : PreviewsLitePagePredictor(web_contents),
        data_saver_enabled_(data_saver_enabled),
        ect_is_slow_(ect_is_slow),
        page_is_blacklisted_(page_is_blacklisted),
        is_visible_(is_visible) {}

  // PreviewsLitePagePredictor:
  bool DataSaverIsEnabled() const override { return data_saver_enabled_; }
  bool ECTIsSlow() const override { return ect_is_slow_; }
  bool PageIsBlacklisted(const GURL& url) const override {
    return page_is_blacklisted_;
  }
  bool IsVisible() const override { return is_visible_; }

  void set_ect_is_slow(bool ect_is_slow) { ect_is_slow_ = ect_is_slow; }
  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }

 private:
  bool data_saver_enabled_;
  bool ect_is_slow_;
  bool page_is_blacklisted_;
  bool is_visible_;
};

class PreviewsLitePagePredictorUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void RunTest(bool feature_enabled,
               bool data_saver_enabled,
               bool ect_is_slow,
               bool page_is_blacklisted,
               bool is_visible) {
    scoped_feature_list_.InitWithFeatureState(
        previews::features::kLitePageServerPreviews, feature_enabled);
    preresolver_.reset(new TestPreviewsLitePagePredictor(
        web_contents(), data_saver_enabled, ect_is_slow, page_is_blacklisted,
        is_visible));
    test_handle_.reset(
        new content::MockNavigationHandle(GURL(kTestUrl), main_rfh()));
    std::vector<GURL> redirect_chain;
    redirect_chain.push_back(GURL(kTestUrl));
    test_handle_->set_redirect_chain(redirect_chain);
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }

  void SimulateWillProcessResponse() { SimulateCommit(); }

  void SimulateCommit() {
    test_handle_->set_has_committed(true);
    test_handle_->set_url(GURL(kTestUrl));
  }

  void CallDidFinishNavigation() {
    preresolver()->DidFinishNavigation(test_handle_.get());
  }

  TestPreviewsLitePagePredictor* preresolver() const {
    return preresolver_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestPreviewsLitePagePredictor> preresolver_;
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
};

TEST_F(PreviewsLitePagePredictorUnitTest, AllConditionsMet_Origin) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.ToggledPreresolve", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", true, 1);
}

TEST_F(PreviewsLitePagePredictorUnitTest, AllConditionsMet_Preview) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(
          PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
              GURL(kTestUrl)));

  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.ToggledPreresolve", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", false, 1);
}

TEST_F(PreviewsLitePagePredictorUnitTest, FeatureDisabled) {
  RunTest(false /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, DataSaverDisabled) {
  RunTest(true /* feature_enabled */, false /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, ECTNotSlow) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          false /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, PageBlacklisted) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, true /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, NotVisible) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          false /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, InsecurePage) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://test.com"));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, ToggleMultipleTimes_Navigations) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", true, 2);
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", true, 2);
}

TEST_F(PreviewsLitePagePredictorUnitTest, ToggleMultipleTimes_ECT) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());

  preresolver()->set_ect_is_slow(false);
  preresolver()->OnEffectiveConnectionTypeChanged(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", true, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", true, 1);
}

TEST_F(PreviewsLitePagePredictorUnitTest, ToggleMultipleTimes_Visibility) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */,
          true /* ect_is_slow */, false /* page_is_blacklisted */,
          true /* is_visible */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());

  preresolver()->set_is_visible(false);
  preresolver()->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", true, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ToggledPreresolve", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", true, 1);
}
