// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/store_update_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

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
void RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return;
  }
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidDecisionTreePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);

  optimization_guide::proto::DecisionTree decision_tree_model =
      optimization_guide::proto::DecisionTree();
  decision_tree_model.set_weight(2.0);

  optimization_guide::proto::TreeNode* tree_node =
      decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  *prediction_model->mutable_model()->mutable_decision_tree() =
      decision_tree_model;
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidEnsemblePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);
  optimization_guide::proto::Ensemble ensemble =
      optimization_guide::proto::Ensemble();
  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *prediction_model->mutable_model()->mutable_ensemble() = ensemble;
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
CreatePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      optimization_guide::proto::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  model_info->set_optimization_target(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<optimization_guide::proto::ClientModelFeature>&
        client_model_features) {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::HostModelFeatures* host_model_features =
        get_models_response->add_host_model_features();
    host_model_features->set_host(host);
    optimization_guide::proto::ModelFeature* model_feature =
        host_model_features->add_model_features();
    model_feature->set_feature_name("host_feat1");
    model_feature->set_double_value(2.0);
  }

  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      CreatePredictionModel();
  for (const auto& client_model_feature : client_model_features) {
    prediction_model->mutable_model_info()->add_supported_model_features(
        client_model_feature);
  }
  prediction_model->mutable_model_info()->set_version(2);
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

enum class PredictionModelsFetcherRemoteResponseType {
  kSuccessfulWithModelsAndFeatures = 0,
  kSuccessfulWithFeaturesAndNoModels = 1,
  kSuccessfulWithModelsAndNoFeatures = 2,
  kUnsuccessful = 3,
};

// A WebContentsObserver that asks whether an optimization target can be
// applied.
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
    service->ShouldTargetNavigation(
        navigation_handle,
        optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  }
};

}  // namespace

namespace optimization_guide {

class PredictionManagerBrowserTest : public InProcessBrowserTest {
 public:
  PredictionManagerBrowserTest() = default;
  ~PredictionManagerBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kRemoteOptimizationGuideFetching,
         optimization_guide::features::kOptimizationTargetPrediction},
        {});

    models_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    models_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    models_server_->RegisterRequestHandler(base::BindRepeating(
        &PredictionManagerBrowserTest::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_.reset(new OptimizationGuideConsumerWebContentsObserver(
        browser()->tab_strip_model()->GetActiveWebContents()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(models_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch(optimization_guide::switches::
                          kFetchModelsAndHostModelFeaturesOverrideTimer);

    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetModelsURL,
        models_server_
            ->GetURL(GURL(optimization_guide::
                              kOptimizationGuideServiceGetModelsDefaultURL)
                         .host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII("force-variation-ids", "4");
  }

  void SetResponseType(
      PredictionModelsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypesAndTargets(
            {optimization_guide::proto::NOSCRIPT},
            {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  }

  PredictionManager* GetPredictionManager() {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  GURL https_url_with_content() { return https_url_with_content_; }
  GURL https_url_without_content() { return https_url_without_content_; }

 protected:
  // Feature that the model server should return in response to
  // GetModelsRequest.
  proto::ClientModelFeature client_model_feature_ =
      optimization_guide::proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response.reset(new net::test_server::BasicHttpResponse);
    // The request to the remote Optimization Guide Service should always be a
    // POST.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));
    response->set_code(net::HTTP_OK);
    std::unique_ptr<optimization_guide::proto::GetModelsResponse>
        get_models_response = BuildGetModelsResponse(
            {"example1.com", https_server_->GetURL("/").host()},
            {client_model_feature_});
    if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                              kSuccessfulWithFeaturesAndNoModels) {
      get_models_response->clear_models();
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithModelsAndNoFeatures) {
      get_models_response->clear_host_model_features();
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    std::string serialized_request;
    get_models_response->SerializeToString(&serialized_request);
    response->set_content(serialized_request);
    return std::move(response);
  }

  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PredictionModelsFetcherRemoteResponseType response_type_ =
      PredictionModelsFetcherRemoteResponseType::
          kSuccessfulWithModelsAndFeatures;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;

  DISALLOW_COPY_AND_ASSIGN(PredictionManagerBrowserTest);
};

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(FCPReachedSessionStatisticsUpdated)) {
  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      GetPredictionManager()->GetFCPSessionStatisticsForTesting();
  EXPECT_TRUE(session_fcp);
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoFCPSessionStatisticsUnchanged)) {
  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      GetPredictionManager()->GetFCPSessionStatisticsForTesting();
  float current_mean = session_fcp->GetMean();

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), https_url_without_content());
  waiter->Wait();
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
  EXPECT_EQ(current_mean, session_fcp->GetMean());
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ModelsAndFeaturesStoreInitialized)) {
  base::HistogramTester histogram_tester;
  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);

  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(OnlyHostModelFeaturesInGetModelsResponse)) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithFeaturesAndNoModels);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(OnlyPredictionModelsInGetModelsResponse)) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndNoFeatures);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  // A metadata entry will always be stored for host model features, regardless
  // of whether any host model features were actually returned.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PredictionModelFetchFailed)) {
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
}

IN_PROC_BROWSER_TEST_F(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HostModelFeaturesClearedOnHistoryClear)) {
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", true,
      1);

  // Wipe the browser history - clears all the host model features.
  browser()->profile()->Wipe();
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ClearHostModelFeatures.StoreAvailable", true, 1);

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", false,
      1);
}

class PredictionManagerBrowserSameOriginTest
    : public PredictionManagerBrowserTest {
 public:
  PredictionManagerBrowserSameOriginTest() = default;
  ~PredictionManagerBrowserSameOriginTest() override = default;

  void SetUp() override {
    client_model_feature_ =
        optimization_guide::proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION;
    PredictionManagerBrowserTest::SetUp();
  }
};

// Regression test for https://crbug.com/1037945. Tests that the origin of the
// previous navigation is computed correctly.
IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserSameOriginTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(IsSameOriginNavigation)) {
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 1);

  // Navigate to the same URL in the same tab. This should count as a
  // same-origin navigation.
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", true, 1);

  // Navigate to a cross-origin URL. This should count as a cross-origin
  // navigation.
  ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com/"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", true, 1);
}

}  // namespace optimization_guide
