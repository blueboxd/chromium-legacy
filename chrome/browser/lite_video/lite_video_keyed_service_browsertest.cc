// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_keyed_service.h"

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run, there might be two
// profiles created, and this will return the total sample count across
// profiles.
int GetTotalHistogramSamples(const base::HistogramTester& histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
  }
}

}  // namespace

class LiteVideoKeyedServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  LiteVideoKeyedServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature({::features::kLiteVideo});
  }
  ~LiteVideoKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceDisabledBrowserTest,
                       KeyedServiceEnabledButLiteVideoDisabled) {
  EXPECT_EQ(nullptr,
            LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
}

class LiteVideoKeyedServiceBrowserTest
    : public LiteVideoKeyedServiceDisabledBrowserTest {
 public:
  LiteVideoKeyedServiceBrowserTest() = default;
  ~LiteVideoKeyedServiceBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {::features::kLiteVideo},
        {{"lite_video_origin_hints", "{\"litevideo.com\": 123}"}});
    InProcessBrowserTest::SetUp();
  }

  lite_video::LiteVideoDecider* lite_video_decider() {
    return LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile())
        ->lite_video_decider();
  }

  void WaitForBlocklistToBeLoaded() {
    EXPECT_GT(
        RetryForHistogramUntilCountReached(
            histogram_tester_, "LiteVideo.UserBlocklist.BlocklistLoaded", 1),
        0);
  }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoEnabledWithKeyedService) {
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_UnsupportedScheme) {
  WaitForBlocklistToBeLoaded();

  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://testserver.com"));

  histogram_tester()->ExpectTotalCount("LiteVideo.Navigation.HasHint", 0);
}

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_NoHintForHost) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), GURL("https://testserver.com"));

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_HasHint) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", true,
                                         1);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}
