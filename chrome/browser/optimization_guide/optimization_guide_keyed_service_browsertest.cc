// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_saver/data_saver_top_host_provider.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

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

// A WebContentsObserver that asks whether an optimization type can be applied.
class OptimizationGuideConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  OptimizationGuideConsumerWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OptimizationGuideConsumerWebContentsObserver() override = default;

  // contents::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    service->CanApplyOptimization(
        navigation_handle,
        optimization_guide::OptimizationTarget::kPainfulPageLoad,
        optimization_guide::proto::NOSCRIPT, /*optimization_metadata=*/nullptr);
  }
};

}  // namespace

using OptimizationGuideKeyedServiceDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceDisabledBrowserTest,
                       KeyedServiceNotEnabledButOptimizationHintsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHints},
      {optimization_guide::features::kOptimizationGuideKeyedService});

  EXPECT_EQ(nullptr, OptimizationGuideKeyedServiceFactory::GetForProfile(
                         browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceDisabledBrowserTest,
                       KeyedServiceEnabledButOptimizationHintsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationGuideKeyedService},
      {optimization_guide::features::kOptimizationHints});

  EXPECT_EQ(nullptr, OptimizationGuideKeyedServiceFactory::GetForProfile(
                         browser()->profile()));
}

class OptimizationGuideKeyedServiceBrowserTest
    : public OptimizationGuideKeyedServiceDisabledBrowserTest {
 public:
  OptimizationGuideKeyedServiceBrowserTest() = default;
  ~OptimizationGuideKeyedServiceBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationGuideKeyedService},
        {});

    OptimizationGuideKeyedServiceDisabledBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::kPurgeHintCacheStore);
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceDisabledBrowserTest::SetUpOnMainThread();

    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideKeyedServiceBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    url_with_hints_ =
        https_server_->GetURL("somehost.com", "/hashints/whatever");
    url_that_redirects_ = https_server_->GetURL("/redirect");

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_.reset(new OptimizationGuideConsumerWebContentsObserver(
        browser()->tab_strip_model()->GetActiveWebContents()));

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  }

  void TearDown() override {
    scoped_feature_list_.Reset();

    OptimizationGuideKeyedServiceDisabledBrowserTest::TearDown();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    OptimizationGuideKeyedServiceDisabledBrowserTest::TearDownOnMainThread();
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
  }

  void PushHintsComponentAndWaitForCompletion() {
    base::RunLoop run_loop;
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->GetHintsManager()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {url_with_hints_.host()}, "*",
            {});

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    run_loop.Run();
  }

  GURL url_with_hints() { return url_with_hints_; }

  GURL url_that_redirects() { return url_that_redirects_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.GetURL().spec().find("redirect") != std::string::npos) {
      response.reset(new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", url_with_hints().spec());
    }
    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL url_with_hints_;
  GURL url_that_redirects_;
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       TopHostProviderNotSetIfNotAllowed) {
  ASSERT_FALSE(
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->GetTopHostProvider());
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageWithHintsButNoRegistrationDoesNotAttemptToLoadHint) {
  PushHintsComponentAndWaitForCompletion();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintsLoadsHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  EXPECT_GT(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);

  // Expect that the optimization guide UKM was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      123);
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageThatRedirectsToUrlWithHintsShouldAttemptTwoLoads) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_that_redirects());

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 2),
            2);
  // Should attempt and fail to load a hint for the initial navigation.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     false, 1);
  // Should attempt and succeed to load a hint once for the redirect.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     true, 1);
  // Expect that the optimization guide UKM was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      123);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithoutHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/"));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There were no hints that match this URL, but there should still be an
  // attempt to load a hint but still fail.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
  // Should expect that no hints were loaded and so we don't have a hint
  // version recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintWithNoVersion) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL("https://m.noversion.com/"));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There should be a hint that matches this URL.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // Should expect that UKM was not recorded since it did not have a version.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintWithBadVersion) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL("https://m.badversion.com/"));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There should be a hint that matches this URL.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // Should expect that UKM was not recorded since it had a bad version string.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

class OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest
    : public OptimizationGuideKeyedServiceBrowserTest {
 public:
  OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest() = default;
  ~OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest() override =
      default;

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceBrowserTest::SetUpOnMainThread();

    SeedSiteEngagementService();
    // Set the blacklist state to initialized so the sites in the engagement
    // service will be used and not blacklisted on the first GetTopHosts
    // request.
    InitializeDataSaverTopHostBlacklist();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceBrowserTest::SetUpCommandLine(cmd);

    cmd->AppendSwitch("enable-spdy-proxy-auth");
    // Add switch to avoid having to see the infobar in the test.
    cmd->AppendSwitch(previews::switches::kDoNotRequireLitePageRedirectInfoBar);
  }

 private:
  // Seeds the Site Engagement Service with two HTTP and two HTTPS sites for the
  // current profile.
  void SeedSiteEngagementService() {
    SiteEngagementService* service = SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    GURL https_url1("https://myfavoritesite.com/");
    service->AddPointsForTesting(https_url1, 15);

    GURL https_url2("https://myotherfavoritesite.com/");
    service->AddPointsForTesting(https_url2, 3);
  }

  void InitializeDataSaverTopHostBlacklist() {
    Profile::FromBrowserContext(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetBrowserContext())
        ->GetPrefs()
        ->SetInteger(optimization_guide::prefs::
                         kHintsFetcherDataSaverTopHostBlacklistState,
                     static_cast<int>(
                         optimization_guide::prefs::
                             HintsFetcherTopHostBlacklistState::kInitialized));
  }
};

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest,
    TopHostProviderIsSentDown) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  optimization_guide::TopHostProvider* top_host_provider =
      keyed_service->GetTopHostProvider();
  ASSERT_TRUE(top_host_provider);

  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts(1);
  EXPECT_EQ(1ul, top_hosts.size());
  EXPECT_EQ("myfavoritesite.com", top_hosts[0]);
}

class OptimizationGuideKeyedServiceCommandLineOverridesTest
    : public OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest {
 public:
  OptimizationGuideKeyedServiceCommandLineOverridesTest() = default;
  ~OptimizationGuideKeyedServiceCommandLineOverridesTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest::
        SetUpCommandLine(cmd);

    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
  }
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceCommandLineOverridesTest,
                       TopHostProviderIsSentDown) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  optimization_guide::TopHostProvider* top_host_provider =
      keyed_service->GetTopHostProvider();
  ASSERT_TRUE(top_host_provider);

  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts(1);
  EXPECT_EQ(1ul, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
}

// TODO(crbug/969558): Migrate this test directly to the HintsFetcherBrowserTest
// when it supports both the original and OptimizationGuideKeyedService path.
class OptimizationGuideKeyedServiceHintsFetcherTest
    : public OptimizationGuideKeyedServiceCommandLineOverridesTest {
 public:
  OptimizationGuideKeyedServiceHintsFetcherTest() = default;
  ~OptimizationGuideKeyedServiceHintsFetcherTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHintsFetching}, {});

    api_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    api_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    api_server_->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideKeyedServiceHintsFetcherTest::HandleGetHintsRequest,
        base::Unretained(this)));
    ASSERT_TRUE(api_server_->InitializeAndListen());

    // We run the base class's set up after we set up here since the base class
    // runs SetUpCommandLine prior to all the API server initialization and
    // will have a non-existent URL to override the API server URL with.
    OptimizationGuideKeyedServiceCommandLineOverridesTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceCommandLineOverridesTest::SetUpCommandLine(
        cmd);

    cmd->AppendSwitch(optimization_guide::switches::kFetchHintsOverrideTimer);
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceURL,
        api_server_->base_url().spec());
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceCommandLineOverridesTest::SetUpOnMainThread();

    api_server_->StartAcceptingConnections();

    // Expect that the browser initialization will record at least one sample
    // in each of the follow histograms as OnePlatform Hints are enabled.
    EXPECT_EQ(
        RetryForHistogramUntilCountReached(
            histogram_tester_,
            "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
        1);

    // There should be 2 sites passed via command line.
    histogram_tester_.ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);

    EXPECT_EQ(RetryForHistogramUntilCountReached(
                  histogram_tester_,
                  "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
              1);
    // There should have been 1 hint returned in the response.
    histogram_tester_.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

    // Wait until fetched hints have been stored.
    EXPECT_EQ(
        RetryForHistogramUntilCountReached(
            histogram_tester_, "OptimizationGuide.FetchedHints.Stored", 1),
        1);
  }

  void TearDown() override {
    // Make sure to reset the other feature list first, otherwise we hit a
    // DCHECK where the feature lists aren't reset in the same order they are
    // set up.
    OptimizationGuideKeyedServiceCommandLineOverridesTest::TearDown();

    feature_list_.Reset();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(api_server_->ShutdownAndWaitUntilComplete());

    OptimizationGuideKeyedServiceCommandLineOverridesTest::
        TearDownOnMainThread();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response.reset(new net::test_server::BasicHttpResponse);
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    response->set_code(net::HTTP_OK);

    optimization_guide::proto::GetHintsResponse get_hints_response;
    optimization_guide::proto::Version hint_version;
    hint_version.mutable_generation_timestamp()->set_seconds(234);
    hint_version.set_hint_source(
        optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
    std::string hint_version_string;
    hint_version.SerializeToString(&hint_version_string);
    base::Base64Encode(hint_version_string, &hint_version_string);

    optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_key("somehost.com");
    hint->set_version(hint_version_string);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");

    std::string serialized_request;
    get_hints_response.SerializeToString(&serialized_request);
    response->set_content(serialized_request);

    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> api_server_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// TODO(crbug/969558): Figure out why hints fetcher not fetching on ChromeOS.
#if defined(OS_CHROMEOS)
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceHintsFetcherTest,
                       DISABLE_ON_CHROMEOS(ClearFetchedHints)) {
  PushHintsComponentAndWaitForCompletion();

  RegisterWithKeyedService();

  // Prompt the loading of the hint that was just fetched.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), url_with_hints());
    EXPECT_EQ(RetryForHistogramUntilCountReached(
                  histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
              1);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                        true, 1);

    // Verifies that the fetched hint is loaded and not the component hint as
    // fetched hints are prioritized.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintCache.HintType.Loaded",
        static_cast<int>(
            optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
        1);
    // Expect that the optimization guide UKM was recorded.
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries.at(0);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::OptimizationGuide::kHintSourceName,
        static_cast<int>(
            optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
        234);
  }

  // Wipe the browser history - clear all the fetched hints.
  browser()->profile()->Wipe();
  // Run until idle so the hints have time to clear and the hint keys are
  // repopulated.
  base::RunLoop().RunUntilIdle();

  // Try to load the same hint to confirm fetched hints are no longer there.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    base::HistogramTester histogram_tester;

    ui_test_utils::NavigateToURL(browser(), url_with_hints());
    EXPECT_EQ(RetryForHistogramUntilCountReached(
                  histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
              1);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                        true, 1);

    // Component Hint should be used instead.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintCache.HintType.Loaded",
        static_cast<int>(
            optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
        1);
    // Expect that the optimization guide UKM was recorded.
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries.at(0);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::OptimizationGuide::kHintSourceName,
        static_cast<int>(optimization_guide::proto::
                             HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
        123);
  }
}
