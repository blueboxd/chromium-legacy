// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_predictor.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class TestPreviewsLitePagePredictor : public PreviewsLitePagePredictor {
 public:
  TestPreviewsLitePagePredictor(content::WebContents* web_contents,
                                bool data_saver_enabled)
      : PreviewsLitePagePredictor(web_contents),
        data_saver_enabled_(data_saver_enabled) {}

  // PreviewsLitePagePredictor:
  bool DataSaverIsEnabled() const override { return data_saver_enabled_; }

 private:
  bool data_saver_enabled_;
};

class PreviewsLitePagePredictorUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void RunTest(bool feature_enabled, bool data_saver_enabled) {
    scoped_feature_list_.InitWithFeatureState(
        previews::features::kLitePageServerPreviews, feature_enabled);
    preresolver_.reset(
        new TestPreviewsLitePagePredictor(web_contents(), data_saver_enabled));
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

TEST_F(PreviewsLitePagePredictorUnitTest, AllConditionsMet) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */);

  base::HistogramTester histogram_tester;

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_TRUE(preresolver()->ShouldPreresolveOnPage());
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.ToggledPreresolve", true, 1);
}

TEST_F(PreviewsLitePagePredictorUnitTest, FeatureDisabled) {
  RunTest(false /* feature_enabled */, true /* data_saver_enabled */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, DataSaverDisabled) {
  RunTest(true /* feature_enabled */, false /* data_saver_enabled */);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, NoNavigation) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */);

  EXPECT_FALSE(preresolver()->ShouldPreresolveOnPage());
}

TEST_F(PreviewsLitePagePredictorUnitTest, ToggleMultipleTimes_Navigations) {
  RunTest(true /* feature_enabled */, true /* data_saver_enabled */);

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
}
