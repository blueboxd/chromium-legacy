// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

}  // namespace

class DeferAllScriptBrowserTest : public InProcessBrowserTest {
 public:
  DeferAllScriptBrowserTest() = default;
  ~DeferAllScriptBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews,
         previews::features::kDeferAllScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/defer_all_script_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  }

  // Creates hint data from the |component_info| and waits for it to be fully
  // processed before returning.
  void ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& component_info) {
    // Register a QuitClosure for when the next hint update is started below.
    base::RunLoop run_loop;
    PreviewsServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()))
        ->previews_ui_service()
        ->previews_decider_impl()
        ->previews_opt_guide()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);
    run_loop.Run();
  }

  // Performs a navigation to |url| and waits for the the url's host's hints to
  // load before returning. This ensures that the hints will be available in the
  // hint cache for a subsequent navigation to a test url with the same host.
  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to the url to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        previews::kPreviewsOptimizationGuideOnLoadedHintResultHistogramString,
        1);
  }

  void SetDeferAllScriptHintWithPageWithPattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::DEFER_ALL_SCRIPT,
            {hint_setup_url.host()}, page_pattern, {}));
    LoadHintsForUrl(hint_setup_url);
  }

  virtual const GURL& https_url() const { return https_url_; }

  std::string GetScriptLog() {
    std::string script_log;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), "sendLogToTest()",
        &script_log));
    return script_log;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL https_url_;

  DISALLOW_COPY_AND_ASSIGN(DeferAllScriptBrowserTest);
};

// Avoid flakes and issues on non-applicable platforms.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(DeferAllScriptHttpsWhitelisted)) {

  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(
      "ScriptLog:_BodyEnd_InlineScript_SyncScript_DeveloperDeferScript_OnLoad",
      GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 1);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe.External", 1, 1);

  // Verify UKM force deferred count entries.
  using UkmDeferEntry = ukm::builders::PreviewsDeferAllScript;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmDeferEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmDeferEntry::kforce_deferred_scripts_mainframeName, 2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmDeferEntry::kforce_deferred_scripts_mainframe_externalName, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(DeferAllScriptHttpsNotWhitelisted)) {

  GURL url = https_url();

  // Whitelist DeferAllScript for the url's host but with nonmatching pattern.
  SetDeferAllScriptHintWithPageWithPattern(url, "/NoMatch/");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // The URL is not whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(
      "ScriptLog:_InlineScript_SyncScript_BodyEnd_DeveloperDeferScript_OnLoad",
      GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(
          previews::PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 0);
}

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(
        DeferAllScriptHttpsWhitelistedButWithCoinFlipHoldback)) {
  // Holdback the page load from previews and also disable offline previews to
  // ensure that only post-commit previews are enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kCoinFlipHoldback,
        {{"force_coin_flip_always_holdback", "true"}}}},
      {previews::features::kOfflinePreviews});

  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(
      "ScriptLog:_InlineScript_SyncScript_BodyEnd_DeveloperDeferScript_OnLoad",
      GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 0);

  // Verify UKM entries.
  using UkmEntry = ukm::builders::Previews;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kcoin_flip_resultName,
                                      2);
  test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kpreviews_likelyName, 1);
  test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kdefer_all_scriptName,
                                      true);
}
