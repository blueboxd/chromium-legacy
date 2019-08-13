// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

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

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideKeyedServiceBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    url_with_hints_ =
        embedded_test_server()->GetURL("somehost.com", "/hashints/whatever");

    PushHintsComponentAndWaitForCompletion();

    OptimizationGuideKeyedServiceDisabledBrowserTest::SetUpOnMainThread();
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
  }

  GURL url_with_hints() { return url_with_hints_; }

 private:
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

  GURL url_with_hints_;
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

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
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintsLoadsHint) {
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  EXPECT_GT(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageThatRedirectsToUrlWithHintsShouldAttemptTwoLoads) {
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  GURL first_url = embedded_test_server()->GetURL("/redirect");
  ui_test_utils::NavigateToURL(browser(), first_url);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 2),
            2);
  // Should attempt and fail to load a hint for the initial navigation.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     false, 1);
  // Should attempt and succeed to load a hint once for the redirect.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     true, 1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithoutHint) {
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/"));

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There were no hints that match this URL, but there should still be an
  // attempt to load a hint but still fail.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
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
                           "whatever.com,awesome.com");
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
