// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"
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
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kMockHost[] = "mock.host";

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

// This test class sets up everything but does not enable any features.
class ResourceLoadingNoFeaturesBrowserTest : public InProcessBrowserTest {
 public:
  ResourceLoadingNoFeaturesBrowserTest() = default;
  ~ResourceLoadingNoFeaturesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
    // Set up https server with resource monitor.
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/resource_loading_hints.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_iframe_ =
        https_server_->GetURL("/resource_loading_hints_iframe.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_iframe_preload_ =
        https_server_->GetURL("/resource_loading_hints_iframe_preload.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_preload_ =
        https_server_->GetURL("/resource_loading_hints_preload.html");

    https_second_url_ =
        https_server_->GetURL("/resource_loading_hints_second.html");
    ASSERT_TRUE(https_second_url_.SchemeIs(url::kHttpsScheme));

    https_no_transform_url_ = https_server_->GetURL(
        "/resource_loading_hints_with_no_transform_header.html");
    ASSERT_TRUE(https_no_transform_url_.SchemeIs(url::kHttpsScheme));

    https_hint_setup_url_ = https_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_hint_setup_url_.SchemeIs(url::kHttpsScheme));
    ASSERT_EQ(https_hint_setup_url_.host(), https_url_.host());

    // Set up http server with resource monitor and redirect handler.
    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    http_server_->RegisterRequestMonitor(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::HandleRedirectRequest,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("/resource_loading_hints.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    redirect_url_ = http_server_->GetURL("/redirect.html");
    ASSERT_TRUE(redirect_url_.SchemeIs(url::kHttpScheme));

    http_hint_setup_url_ = http_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(http_hint_setup_url_.SchemeIs(url::kHttpScheme));
    ASSERT_EQ(http_hint_setup_url_.host(), http_url_.host());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void InitializeOptimizationHints() {
    std::unique_ptr<optimization_guide::proto::Configuration> config =
        std::make_unique<optimization_guide::proto::Configuration>();
    std::unique_ptr<previews::PreviewsHints> hints =
        previews::PreviewsHints::CreateFromHintsConfiguration(std::move(config),
                                                              nullptr);

    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());

    previews_service->previews_ui_service()
        ->previews_decider_impl()
        ->previews_opt_guide()
        ->UpdateHints(base::DoNothing(), std::move(hints));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("optimization-guide-disable-installer");

    // Needed so that the browser can connect to kMockHost.
    cmd->AppendSwitch("ignore-certificate-errors");

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

    // Navigate to |hint_setup_url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        previews::kPreviewsOptimizationGuideOnLoadedHintResultHistogramString,
        1);
  }

  void SetDefaultOnlyResourceLoadingHints(const GURL& hint_setup_url) {
    SetDefaultOnlyResourceLoadingHintsWithPagePattern(hint_setup_url, "*");
  }

  void SetDefaultOnlyResourceLoadingHintsWithPagePattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING,
            {hint_setup_url.host(), kMockHost}, page_pattern,
            resource_patterns));
    LoadHintsForUrl(hint_setup_url);
  }

  // Sets the resource loading hints in optimization guide service. The hints
  // are set as experimental.
  void SetExperimentOnlyResourceLoadingHints(const GURL& hint_setup_url) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_
            .CreateHintsComponentInfoWithExperimentalPageHints(
                optimization_guide::proto::RESOURCE_LOADING,
                {hint_setup_url.host()}, resource_patterns));
    LoadHintsForUrl(hint_setup_url);
  }

  // Sets the resource loading hints in optimization guide service. Some hints
  // are set as experimental, while others are set as default.
  void SetMixResourceLoadingHints(const GURL& hint_setup_url) {
    std::vector<std::string> experimental_resource_patterns;
    experimental_resource_patterns.push_back("foo.jpg");
    experimental_resource_patterns.push_back("png");
    experimental_resource_patterns.push_back("woff2");

    std::vector<std::string> default_resource_patterns;
    default_resource_patterns.push_back("bar.jpg");
    default_resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithMixPageHints(
            optimization_guide::proto::RESOURCE_LOADING,
            {hint_setup_url.host()}, experimental_resource_patterns,
            default_resource_patterns));
    LoadHintsForUrl(hint_setup_url);
  }

  virtual const GURL& https_url() const { return https_url_; }

  // URL that loads blocked resources uses link-rel preload in <head>.
  const GURL& https_url_preload() const { return https_url_preload_; }

  // URL that loads a woff2 resource and https_url() webpage in an iframe.
  virtual const GURL& https_url_iframe() const { return https_url_iframe_; }

  // URL that loads a woff2 resource and https_url_preload() webpage in an
  // iframe.
  virtual const GURL& https_url_iframe_preload() const {
    return https_url_iframe_preload_;
  }

  const GURL& https_second_url() const { return https_second_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& https_hint_setup_url() const { return https_hint_setup_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  const GURL& http_hint_setup_url() const { return http_hint_setup_url_; }

  void SetExpectedFooJpgRequest(bool expect_foo_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/foo.jpg"] = expect_foo_jpg_requested;
  }
  void SetExpectedBarJpgRequest(bool expect_bar_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/bar.jpg"] = expect_bar_jpg_requested;
  }
  void SetExpectedBazWoff2Request(bool expect_woff_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/baz.woff2"] = expect_woff_requested;
  }

  int GetProcessID() const {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->GetProcess()
        ->GetID();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(http_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ResourceLoadingNoFeaturesBrowserTest::
                           MonitorResourceRequestOnUIThread,
                       base::Unretained(this), request.GetURL()));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.GetURL().spec().find("redirect") != std::string::npos) {
      response.reset(new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", https_url().spec());
    }
    return std::move(response);
  }

  void MonitorResourceRequestOnUIThread(const GURL& gurl) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for (const auto& expect : subresource_expected_) {
      if (gurl.path() == expect.first) {
        // Verify that |gurl| is expected to be fetched. This ensures that a
        // resource whose loading is blocked is not loaded.
        EXPECT_TRUE(expect.second)
            << " GURL " << gurl
            << " was expected to be blocked, but was actually fetched";
        // Subresource should not be fetched again.
        subresource_expected_[gurl.path()] = false;
        return;
      }
    }
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  GURL https_url_;
  GURL https_url_preload_;
  GURL https_url_iframe_;
  GURL https_url_iframe_preload_;
  GURL https_second_url_;
  GURL https_no_transform_url_;
  GURL https_hint_setup_url_;
  GURL http_url_;
  GURL redirect_url_;
  GURL http_hint_setup_url_;

  // Mapping from a subresource path to whether the resource is expected to be
  // fetched. Once a subresource present in this map is fetched, the
  // corresponding value is set to false.
  // Must only be accessed on UI thread.
  std::map<std::string, bool> subresource_expected_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingNoFeaturesBrowserTest);
};

// This test class enables ResourceLoadingHints with OptimizationHints.
// First parameter is true if the test should be run with a webpage that
// preloads resources in the HTML head using link-rel preload.
// Second parameter is true if the blink feature
// kSendPreviewsLoadingHintsBeforeCommit should be enabled.
class ResourceLoadingHintsBrowserTest
    : public ::testing::WithParamInterface<std::tuple<bool, bool>>,
      public ResourceLoadingNoFeaturesBrowserTest {
 public:
  ResourceLoadingHintsBrowserTest()
      : use_preload_resources_webpage_(std::get<0>(GetParam())),
        use_render_frame_observer_(std::get<1>(GetParam())) {}

  ~ResourceLoadingHintsBrowserTest() override = default;

  void SetUp() override {
    // Enabling NoScript should have no effect since resource loading takes
    // priority over NoScript.
    if (!use_render_frame_observer_) {
      scoped_feature_list_.InitWithFeatures(
          {previews::features::kPreviews, previews::features::kNoScriptPreviews,
           optimization_guide::features::kOptimizationHints,
           previews::features::kResourceLoadingHints,
           data_reduction_proxy::features::
               kDataReductionProxyEnabledWithNetworkService},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kSendPreviewsLoadingHintsBeforeCommit,
           previews::features::kPreviews, previews::features::kNoScriptPreviews,
           optimization_guide::features::kOptimizationHints,
           previews::features::kResourceLoadingHints,
           data_reduction_proxy::features::
               kDataReductionProxyEnabledWithNetworkService},
          {});
    }
    ResourceLoadingNoFeaturesBrowserTest::SetUp();
  }

  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) const {
    return server.GetURL(kMockHost, relative_url);
  }

  const GURL& https_url() const override {
    if (use_preload_resources_webpage_)
      return ResourceLoadingNoFeaturesBrowserTest::https_url_preload();
    return ResourceLoadingNoFeaturesBrowserTest::https_url();
  }

  const GURL& https_url_iframe() const override {
    if (use_preload_resources_webpage_)
      return ResourceLoadingNoFeaturesBrowserTest::https_url_iframe_preload();
    return ResourceLoadingNoFeaturesBrowserTest::https_url_iframe();
  }

  bool use_preload_resources_webpage() const {
    return use_preload_resources_webpage_;
  }

  bool use_render_frame_observer() const { return use_render_frame_observer_; }

 private:
  const bool use_preload_resources_webpage_;
  const bool use_render_frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsBrowserTest);
};

// First parameter is true if the test should be run with a webpage that
// preloads resources in the HTML head using link-rel preload. Second parameter
// is true if the blink feature kSendPreviewsLoadingHintsBeforeCommit
// should be enabled.
INSTANTIATE_TEST_SUITE_P(,
                         ResourceLoadingHintsBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

// Previews InfoBar (which these tests triggers) does not work on Mac.
// See https://crbug.com/782322 for details. Also occasional flakes on win7
// (https://crbug.com/789542).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) x
#endif

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(ResourceLoadingHintsHttpsWhitelisted)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 0);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectBucketCount(
      "Previews.PreviewShown.ResourceLoadingHints", true, 1);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);

  // Load the same webpage to ensure that the resource loading hints are sent
  // again.
  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      2);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 2);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 0);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 2);
  histogram_tester.ExpectBucketCount(
      "Previews.PreviewShown.ResourceLoadingHints", true, 2);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 2);

  int previous_process_id = GetProcessID();

  {
    // Navigate to a cross-origin URL. Ensure that (i) there is no browser
    // crash; and (ii) web contents observer sends the hints to the correct
    // agent on the renderer side.
    SetExpectedFooJpgRequest(false);
    SetExpectedBarJpgRequest(true);

    base::HistogramTester histogram_tester_2;
    ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithMockHost(*https_server_, "/resource_loading_hints.html"));

    int current_process_id = GetProcessID();
    EXPECT_NE(previous_process_id, current_process_id);
    base::RunLoop().RunUntilIdle();
    RetryForHistogramUntilCountReached(
        &histogram_tester_2,
        "ResourceLoadingHints.CountBlockedSubresourcePatterns", 1);
  }
}

// The test loads https_url_iframe() which is whitelisted for resource blocking.
// This webpage loads a woff2 resource whose loading should be blocked.
// https_url_iframe() also loads https_url() in an iframe. https_url()
// contains two resources whose URL match blocked patterns. However, since
// https_url() is inside an iframe, loading of those two resources should not
// be blocked.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(ResourceLoadingHintsHttpsWhitelisted_Iframe)) {
  GURL url = https_url_iframe();

  // Whitelist resource loading hints for https_url_iframe()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_url_iframe());

  // Loading of these two resources should not be blocked since they are loaded
  // by a webpage inside an iframe.
  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  // Woff2 subresource is loaded by the https_url_iframe() and its loading
  // should be blocked.
  SetExpectedBazWoff2Request(false);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 1);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

// The test loads https_url_iframe() which is NOT whitelisted for resource
// blocking. This webpage loads a woff2 resource whose loading should NOT be
// blocked. https_url_iframe() also loads https_url() in an iframe. https_url()
// contains two resources whose URL match blocked patterns. Loading of those two
// resources should not be blocked either since https_url_iframe() is not
// whitelisted.
IN_PROC_BROWSER_TEST_P(ResourceLoadingHintsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(
                           ResourceLoadingHintsHttpsNotWhitelisted_Iframe)) {
  GURL url = https_url_iframe();

  // Do not whitelist resource loading hints for https_url_iframe()'s' host.

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  // Woff2 subresource is loaded by the https_url_iframe(). The other two
  // resources are loaded by the webpage inside the iframe. None of these
  // resources should be blocked from loading.
  SetExpectedBazWoff2Request(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

// Sets only the experimental hints, but does not enable the matching
// experiment. Verifies that the hints are not used, and the resource loading is
// not blocked.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(ExperimentalHints_ExperimentIsNotEnabled)) {
  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetExperimentOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

// Sets only the experimental hints, and enables the matching experiment.
// Verifies that the hints are used, and the resource loading is blocked.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(ExperimentalHints_ExperimentIsEnabled)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;

  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{optimization_guide::features::kOptimizationHintsExperimentNameParam,
        optimization_guide::testing::kFooExperimentName}});

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetExperimentOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 0);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectBucketCount(
      "Previews.PreviewShown.ResourceLoadingHints", true, 1);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);
}

// Sets both the experimental and default hints, and enables the matching
// experiment. Verifies that the hints are used, and the resource loading is
// blocked.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(MixExperimentalHints_ExperimentIsEnabled)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;

  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{optimization_guide::features::kOptimizationHintsExperimentNameParam,
        optimization_guide::testing::kFooExperimentName}});

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host. Set
  // both experimental and non-experimental hints.
  SetMixResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 0);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectBucketCount(
      "Previews.PreviewShown.ResourceLoadingHints", true, 1);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);
}

// Sets the default hints with a non-wildcard page pattern. Loads a webpage from
// an origin for which the hints are present, but the page pattern does not
// match. Verifies that the hints are not used on that webpage.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(MatchingOrigin_NonMatchingPagePattern)) {
  const GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  // Set pattern to a string that does not matches https_url() path.
  ASSERT_EQ(std::string::npos, https_url().path().find("mismatched_pattern"));
  SetDefaultOnlyResourceLoadingHintsWithPagePattern(https_url(),
                                                    "mismatched_pattern");

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);
  InitializeOptimizationHints();

  base::HistogramTester histogram_tester;

  // The URL is not whitelisted. Verify that the hints are not used.
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(
          previews::PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

// Sets the default hints with a non-wildcard page pattern. First loads a
// webpage from a host for which the hints are present (page pattern matches).
// Next, loads a webpage from the same host but the webpage's URL does not match
// the page patterns. Verifies that the hints are not used on that webpage.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(SameOriginDifferentPattern)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;
  // Whitelist resource loading hints for https_url()'s' host and pattern.
  SetDefaultOnlyResourceLoadingHintsWithPagePattern(https_url(),
                                                    https_url().path());

  // Hints should be used when loading https_url().
  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester_1;

  ui_test_utils::NavigateToURL(browser(), https_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester_1,
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 1);
  histogram_tester_1.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester_1.ExpectTotalCount(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 0);
  histogram_tester_1.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester_1.ExpectBucketCount(
      "Previews.PreviewShown.ResourceLoadingHints", true, 1);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester_1.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);

  // Load a different webpage on the same origin to ensure that the resource
  // loading hints are not reused.
  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);
  base::HistogramTester histogram_tester_2;

  // https_second_url() is hosted on the same host as https_url(), but the path
  // for https_second_url() is not whitelisted.
  ASSERT_EQ(https_url().host(), https_second_url().host());
  ASSERT_NE(https_url().path(), https_second_url().path());

  ui_test_utils::NavigateToURL(browser(), https_second_url());
  base::RunLoop().RunUntilIdle();

  histogram_tester_2.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(
          previews::PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
  histogram_tester_2.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester_2.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

// Sets both the experimental and default hints, but does not enable the
// matching experiment. Verifies that the hints from the experiment are not
// used.
IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(MixExperimentalHints_ExperimentIsNotEnabled)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{optimization_guide::features::kOptimizationHintsExperimentNameParam,
        "some_other_experiment"}});

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetMixResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(false);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  // Infobar would still be shown since there were at least one resource
  // loading hints available, even though none of them matched.
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PreviewShown.ResourceLoadingHints", 1);
}

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(
        ResourceLoadingHintsHttpsWhitelistedRedirectToHttps)) {
  if (use_preload_resources_webpage() && !use_render_frame_observer())
    return;
  GURL url = redirect_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 2);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PreviewShown.ResourceLoadingHints", 1);
  // SetDefaultOnlyResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);
}

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(ResourceLoadingHintsHttpsNotWhitelisted)) {
  GURL url = https_url();

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);
  InitializeOptimizationHints();

  base::HistogramTester histogram_tester;

  // The URL is not whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(
          previews::PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

IN_PROC_BROWSER_TEST_P(ResourceLoadingHintsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(ResourceLoadingHintsHttp)) {
  GURL url = http_url();

  // Whitelist resource loading hints for http_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(http_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

IN_PROC_BROWSER_TEST_P(ResourceLoadingHintsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(
                           ResourceLoadingHintsHttpsWhitelistedNoTransform)) {
  GURL url = https_no_transform_url();

  // Whitelist resource loading hints for http_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(http_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(
        ResourceLoadingHintsHttpsWhitelistedButShouldNotApplyBecauseCoinFlipHoldback)) {
  // Holdback the page load from previews and also disable offline previews to
  // ensure that only post-commit previews are enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kCoinFlipHoldback,
        {{"force_coin_flip_always_holdback", "true"}}}},
      {previews::features::kOfflinePreviews});

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
  // Make sure we did not record a PreviewsResourceLoadingHints UKM for it.
  auto rlh_ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PreviewsResourceLoadingHints::kEntryName);
  ASSERT_EQ(0u, rlh_ukm_entries.size());
}
