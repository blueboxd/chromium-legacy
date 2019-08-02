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
#include "chrome/browser/engagement/site_engagement_service.h"
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
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
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

enum class HintsFetcherRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
};

}  // namespace

// This test class sets up everything but does not enable any features.
class HintsFetcherDisabledBrowserTest : public InProcessBrowserTest {
 public:
  HintsFetcherDisabledBrowserTest() = default;
  ~HintsFetcherDisabledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    origin_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    origin_server_->RegisterRequestHandler(base::BindRepeating(
        &HintsFetcherDisabledBrowserTest::HandleOriginRequest,
        base::Unretained(this)));

    ASSERT_TRUE(origin_server_->Start());

    https_url_ = origin_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_url().SchemeIs(url::kHttpsScheme));

    hints_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    hints_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    hints_server_->RegisterRequestHandler(base::BindRepeating(
        &HintsFetcherDisabledBrowserTest::HandleGetHintsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(hints_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("purge_hint_cache_store");

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceURL,
        hints_server_->base_url().spec());
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "example1.com, example2.com");
  }

  // Creates hint data for the |hint_setup_url|'s so that OnHintsUpdated in
  // Previews Optimization Guide is called and HintsFetch can be tested.
  void SetUpComponentUpdateHints(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, "*",
            {});

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

  void SetNetworkConnectionOffline() {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  // Seeds the Site Engagement Service with two HTTP and two HTTPS sites for the
  // current profile.
  void SeedSiteEngagementService() {
    SiteEngagementService* service = SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    GURL https_url1("https://images.google.com/");
    service->AddPointsForTesting(https_url1, 15);

    GURL https_url2("https://news.google.com/");
    service->AddPointsForTesting(https_url2, 3);

    GURL http_url1("http://photos.google.com/");
    service->AddPointsForTesting(http_url1, 21);

    GURL http_url2("http://maps.google.com/");
    service->AddPointsForTesting(http_url2, 2);
  }

  void SetTopHostBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState
          blacklist_state) {
    Profile::FromBrowserContext(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetBrowserContext())
        ->GetPrefs()
        ->SetInteger(
            optimization_guide::prefs::kHintsFetcherTopHostBlacklistState,
            static_cast<int>(blacklist_state));
  }

  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to |url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        previews::kPreviewsOptimizationGuideOnLoadedHintResultHistogramString,
        1);
  }

  const GURL& https_url() const { return https_url_; }
  const base::HistogramTester* GetHistogramTester() {
    return &histogram_tester_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> hints_server_;
  HintsFetcherRemoteResponseType response_type_ =
      HintsFetcherRemoteResponseType::kSuccessful;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_EQ(request.method, net::test_server::METHOD_GET);
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);

    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response.reset(new net::test_server::BasicHttpResponse);
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    if (response_type_ == HintsFetcherRemoteResponseType::kSuccessful) {
      response->set_code(net::HTTP_OK);

      optimization_guide::proto::GetHintsResponse get_hints_response;

      optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
      hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      hint->set_key(https_url_.host());
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern("page pattern");

      std::string serialized_request;
      get_hints_response.SerializeToString(&serialized_request);
      response->set_content(serialized_request);
    } else if (response_type_ ==
               HintsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);

    } else if (response_type_ == HintsFetcherRemoteResponseType::kMalformed) {
      response->set_code(net::HTTP_OK);

      std::string serialized_request = "Not a proto";
      response->set_content(serialized_request);

    } else {
      NOTREACHED();
    }

    return std::move(response);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(origin_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(hints_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL https_url_;

  base::HistogramTester histogram_tester_;

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherDisabledBrowserTest);
};

// This test class enables OnePlatform Hints.
class HintsFetcherBrowserTest : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherBrowserTest() = default;

  ~HintsFetcherBrowserTest() override = default;

  void SetUp() override {
    // Enable OptimizationHintsFetching with |kOptimizationHintsFetching|.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kNoScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         previews::features::kResourceLoadingHints,
         optimization_guide::features::kOptimizationHintsFetching,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {});
    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherBrowserTest);
};

// This test class enables OnePlatform Hints and configures the test remote
// Optimization Guide Service to return a malformed GetHintsResponse. This
// is for fault testing.
class HintsFetcherWithResponseBrowserTest
    : public ::testing::WithParamInterface<HintsFetcherRemoteResponseType>,
      public HintsFetcherBrowserTest {
 public:
  HintsFetcherWithResponseBrowserTest() = default;

  ~HintsFetcherWithResponseBrowserTest() override = default;

  void SetUp() override {
    response_type_ = GetParam();
    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherWithResponseBrowserTest);
};

// Issues with multiple profiles likely cause the site enagement service-based
// tests to flake.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) x
#endif

// This test creates new browser with no profile and loads a random page with
// the feature flags enables the PreviewsOnePlatformHints. We confirm that the
// top_host_provider_impl executes and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherEnabled)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherDisabledBrowserTest, HintsFetcherDisabled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Expect that the histogram for HintsFetcher to be 0 because the OnePlatform
  // is not enabled.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

// This test creates a new browser and seeds the Site Engagement Service with
// both HTTP and HTTPS sites. The test confirms that PreviewsTopHostProviderImpl
// used by PreviewsOptimizationGuide to provide a list of hosts to HintsFetcher
// only returns HTTPS-schemed hosts. We verify this with the UMA histogram
// logged when the GetHintsRequest is made to the remote Optimization Guide
// Service.
IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(PreviewsTopHostProviderHTTPSOnly)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Adds two HTTP and two HTTPS sites into the Site Engagement Service.
  SeedSiteEngagementService();

  // This forces the hint cache to be initialized and hints to be fetched.
  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample as
  // Hints Fetching is enabled. This also ensures that the histograms have been
  // updated to verify the correct number of hosts that hints will be requested
  // for.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Only the 2 HTTPS hosts should be requested hints for.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherFetchedHintsLoaded)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
      1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
      0);
}

IN_PROC_BROWSER_TEST_P(
    HintsFetcherWithResponseBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherWithResponses)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  const HintsFetcherRemoteResponseType response_type = GetParam();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  if (response_type == HintsFetcherRemoteResponseType::kSuccessful) {
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK,
        1);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
        1);
  } else if (response_type == HintsFetcherRemoteResponseType::kUnsuccessful) {
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.Status",
        net::HTTP_NOT_FOUND, 1);
  } else if (response_type == HintsFetcherRemoteResponseType::kMalformed) {
    // A malformed GetHintsResponse will still register as successful fetch with
    // respect to the network.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK,
        1);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
        1);

    LoadHintsForUrl(https_url());

    ui_test_utils::NavigateToURL(browser(), https_url());

    // Verifies that no Fetched Hint was added to the store, only the
    // Component hint is loaded.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintCache.HintType.Loaded",
        static_cast<int>(
            optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
        1);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintCache.HintType.Loaded",
        static_cast<int>(
            optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
        0);

  } else {
    NOTREACHED();
  }
}

INSTANTIATE_TEST_SUITE_P(
    HintsFetcherWithResponses,
    HintsFetcherWithResponseBrowserTest,
    ::testing::Values(HintsFetcherRemoteResponseType::kSuccessful,
                      HintsFetcherRemoteResponseType::kUnsuccessful,
                      HintsFetcherRemoteResponseType::kMalformed));

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherClearFetchedHints)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as OnePlatform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
      1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
      0);

  // Wipe the browser history - clear all the fetched hints.
  browser()->profile()->Wipe();

  // Try to load the same hint to confirm fetched hints are no longer there.
  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Fetched Hints count should not change.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
      1);

  // Component Hints count should increase.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
      1);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherOverrideTimer)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  SeedSiteEngagementService();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts request.
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as OnePlatform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // There should be 2 sites in the engagement service.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kFetchedHint),
      1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(
          optimization_guide::HintCacheStore::StoreEntryType::kComponentHint),
      0);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(HintsFetcherNetworkOffline)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  // Set the network to be offline.
  SetNetworkConnectionOffline();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts
  // request.
  SeedSiteEngagementService();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts request.
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // No HintsFetch should occur because the connection is offline.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}
