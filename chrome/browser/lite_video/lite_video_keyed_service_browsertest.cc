// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
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
#include "content/public/test/network_connection_change_simulator.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/mojom/network_change_manager.mojom-shared.h"
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
    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
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

class LiteVideoDataSaverDisabledBrowserTest : public InProcessBrowserTest {
 public:
  LiteVideoDataSaverDisabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kLiteVideo);
  }
  ~LiteVideoDataSaverDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LiteVideoDataSaverDisabledBrowserTest,
                       LiteVideoEnabled_DataSaverOff) {
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

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
    SetEffectiveConnectionType(
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
  }

  // Sets the effective connection type that the Network Quality Tracker will
  // report.
  void SetEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(effective_connection_type);
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

// Fails occasionally on ChromeOS. http://crbug.com/1102563
#if defined(OS_CHROMEOS)
#define MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost \
  DISABLED_LiteVideoCanApplyLiteVideo_NoHintForHost
#else
#define MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost \
  LiteVideoCanApplyLiteVideo_NoHintForHost
#endif
IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost) {
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
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
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
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

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_NetworkNotCellular) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);
  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);

  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}

IN_PROC_BROWSER_TEST_F(
    LiteVideoKeyedServiceBrowserTest,
    LiteVideoCanApplyLiteVideo_NetworkConnectionBelowMinECT) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}
