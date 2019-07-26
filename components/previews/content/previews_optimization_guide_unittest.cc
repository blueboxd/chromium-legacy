// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {
// A fake default page_id for testing.
const uint64_t kDefaultPageId = 123456;

enum class HintsFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithHints = 1,
  kFetchSuccessWithNoHints = 2,
};

// Retry delay is 16 minutes to allow for kFetchRetryDelaySecs +
// kFetchRandomMaxDelaySecs to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 16;

constexpr int kUpdateFetchHintsTimeSecs = 24 * 60 * 60;  // 24 hours.

}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : OptimizationGuideService(ui_task_runner),
        add_observer_called_(false),
        remove_observer_called_(false) {}

  void AddObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    add_observer_called_ = true;
  }

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool AddObserverCalled() { return add_observer_called_; }
  bool RemoveObserverCalled() { return remove_observer_called_; }

 private:
  bool add_observer_called_;
  bool remove_observer_called_;
};

// A mock class implementation for unittesting previews_optimization_guide.
class MockTopHostProvider : public optimization_guide::TopHostProvider {
 public:
  MOCK_METHOD1(GetTopHosts, std::vector<std::string>(size_t max_sites));
};

std::unique_ptr<optimization_guide::proto::GetHintsResponse> BuildHintsResponse(
    std::vector<std::string> hosts) {
  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_key(host);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
  }
  return get_hints_response;
}

// A mock class implementation of HintsFetcher for unittesting
// previews_optimization_guide.
class TestHintsFetcher : public optimization_guide::HintsFetcher {
  using HintsFetchedCallback = base::OnceCallback<void(
      base::Optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>)>;
  using HintsFetcher::FetchOptimizationGuideServiceHints;

 public:
  TestHintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      HintsFetcherEndState fetch_state)
      : HintsFetcher(url_loader_factory, optimization_guide_service_url),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      HintsFetchedCallback hints_fetched_callback) override {
    switch (fetch_state_) {
      case HintsFetcherEndState::kFetchFailed:
        std::move(hints_fetched_callback).Run(base::nullopt);
        return false;
      case HintsFetcherEndState::kFetchSuccessWithHints:
        hints_fetched_ = true;
        std::move(hints_fetched_callback).Run(BuildHintsResponse({"host.com"}));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithNoHints:
        hints_fetched_ = true;
        std::move(hints_fetched_callback).Run(BuildHintsResponse({}));
        return true;
    }
    return true;
  }

  bool hints_fetched() { return hints_fetched_; }

 private:
  bool hints_fetched_ = false;
  HintsFetcherEndState fetch_state_;
};

// A Test PreviewsOptimizationGuide to observe and record when callbacks
// from hints fetching and storing occur.
class TestPreviewsOptimizationGuide : public PreviewsOptimizationGuide {
 public:
  TestPreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const base::FilePath& profile_path,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      optimization_guide::TopHostProvider* optimization_guide_top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : PreviewsOptimizationGuide(optimization_guide_service,
                                  ui_task_runner,
                                  background_task_runner,
                                  profile_path,
                                  pref_service,
                                  database_provider,
                                  optimization_guide_top_host_provider,
                                  url_loader_factory) {}

  bool fetched_hints_stored() { return fetched_hints_stored_; }

 private:
  void OnHintsFetched(
      base::Optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response) override {
    fetched_hints_stored_ = false;
    PreviewsOptimizationGuide::OnHintsFetched(std::move(get_hints_response));
  }

  void OnFetchedHintsStored() override {
    fetched_hints_stored_ = true;
    PreviewsOptimizationGuide::OnFetchedHintsStored();
  }

  bool fetched_hints_stored_ = false;
};

class PreviewsOptimizationGuideTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  PreviewsOptimizationGuideTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI,
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {}

  ~PreviewsOptimizationGuideTest() override {}

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();
    EnableDataSaver();
    CreateServiceAndGuide();
  }

  // Delete |guide_| if it hasn't been deleted.
  void TearDown() override {
    ProtoDatabaseProviderTestBase::TearDown();
    ResetGuide();
  }

  PreviewsOptimizationGuide* guide() { return guide_.get(); }

  MockTopHostProvider* top_host_provider() {
    return optimization_guide_top_host_provider_.get();
  }

  TestOptimizationGuideService* optimization_guide_service() {
    return optimization_guide_service_.get();
  }

  network::SharedURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  PrefService* pref_service() { return pref_service_.get(); }

  TestHintsFetcher* hints_fetcher() {
    return static_cast<TestHintsFetcher*>(guide_->GetHintsFetcherForTesting());
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version) {
    base::HistogramTester histogram_tester;

    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));
    base::RunLoop run_loop;
    guide_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    guide_->OnHintsComponentAvailable(info);
    run_loop.Run();

    // Check for histogram to ensure that we do not remove this histogram since
    // it's relied on in release tools.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 1);
  }

  void EnableDataSaver() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  void DisableDataSaver() {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  void CreateServiceAndGuide() {
    if (guide_) {
      ResetGuide();
    }
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    if (!optimization_guide_top_host_provider_) {
      optimization_guide_top_host_provider_ =
          std::make_unique<MockTopHostProvider>();
    }
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            scoped_task_environment_.GetMainThreadTaskRunner());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    // Registry pref for DataSaver with default off.
    pref_service_->registry()->RegisterBooleanPref(
        data_reduction_proxy::prefs::kDataSaverEnabled, false);
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    guide_ = std::make_unique<TestPreviewsOptimizationGuide>(
        optimization_guide_service_.get(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        scoped_task_environment_.GetMainThreadTaskRunner(), temp_dir(),
        pref_service_.get(), db_provider_.get(),
        optimization_guide_top_host_provider_.get(), url_loader_factory_);

    guide_->SetTimeClockForTesting(scoped_task_environment_.GetMockClock());

    base::test::ScopedFeatureList scoped_list;
    scoped_list.InitAndEnableFeature(
        optimization_guide::features::kOptimizationHintsFetching);

    // Add observer is called after the HintCache is fully initialized,
    // indicating that the PreviewsOptimizationGuide is ready to process hints.
    while (!optimization_guide_service_->AddObserverCalled()) {
      RunUntilIdle();
    }
  }

  const base::Clock* GetMockClock() const {
    return scoped_task_environment_.GetMockClock();
  }

  void ResetGuide() {
    guide_.reset();
    RunUntilIdle();
  }

  std::unique_ptr<TestHintsFetcher> BuildTestHintsFetcher(
      HintsFetcherEndState end_state) {
    std::unique_ptr<TestHintsFetcher> hints_fetcher =
        std::make_unique<TestHintsFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"), end_state);
    return hints_fetcher;
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    scoped_task_environment_.FastForwardBy(time_delta);
    base::RunLoop().RunUntilIdle();
  }

  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void DoExperimentFlagTest(base::Optional<std::string> experiment_name,
                            bool expect_enabled);

  // This is a helper function for initializing fixed number of ResourceLoading
  // hints.
  void InitializeFixedCountResourceLoadingHints();

  // This is a helper function for initializing fixed number of ResourceLoading
  // hints that contain an experiment
  void InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  // This is a helper function for initializing multiple ResourceLoading hints.
  // The generated hint proto contains hints for |key_count| keys.
  // |page_patterns_per_key| page patterns are specified per key.
  // For each page pattern, 2 resource loading hints are specified in the proto.
  void InitializeMultipleResourceLoadingHints(size_t key_count,
                                              size_t page_patterns_per_key);

  // This is a helper function for initializing with a LITE_PAGE_REDIRECT
  // server blacklist.
  void InitializeWithLitePageRedirectBlacklist();

  // This function guarantees that all of the asynchronous processing required
  // to load the specified hint has occurred prior to calling IsWhitelisted.
  // It accomplishes this by calling MaybeLoadOptimizationHints() and waiting
  // until OnLoadOptimizationHints runs before calling IsWhitelisted().
  bool MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      PreviewsUserData* previews_data,
      const GURL& url,
      PreviewsType type,
      net::EffectiveConnectionType* out_ect_threshold);

  void RunUntilFetchedHintsStored() {
    while (!guide_->fetched_hints_stored()) {
    }
  }

  // Flag set when the OnLoadOptimizationHints callback runs. This indicates
  // that MaybeLoadOptimizationHints() has completed its processing.
  bool requested_hints_loaded_;

 private:
  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }

  void TestOnHintsFetched(
      std::unique_ptr<optimization_guide::proto::GetHintsResponse>
          get_hints_response) {}

  std::unique_ptr<TestHintsFetcher> test_fetcher_;
  // Callback used to indicate that the asynchronous call to
  // MaybeLoadOptimizationHints() has completed its processing.
  void OnLoadOptimizationHints();

  std::unique_ptr<TestPreviewsOptimizationGuide> guide_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<MockTopHostProvider> optimization_guide_top_host_provider_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;


  GURL loaded_hints_document_gurl_;
  std::vector<std::string> loaded_hints_resource_patterns_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideTest);
};

void PreviewsOptimizationGuideTest::InitializeFixedCountResourceLoadingHints() {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint for "/news/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Page hint for "football"
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("football");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint2 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint2->set_resource_pattern("football_cruft.js");

  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint3 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint3->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint3->set_resource_pattern("barball_cruft.js");

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::
    InitializeFixedCountResourceLoadingHintsWithTwoExperiments() {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  // Page hint for "/news/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");

  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_experiment_name("experiment_1");
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_experiment_name("experiment_2");
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint2 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint2->set_resource_pattern("football_cruft.js");

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::InitializeMultipleResourceLoadingHints(
    size_t key_count,
    size_t page_patterns_per_key) {
  optimization_guide::proto::Configuration config;

  for (size_t key_index = 0; key_index < key_count; ++key_index) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key("somedomain" + base::NumberToString(key_index) + ".org");
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    for (size_t page_pattern_index = 0;
         page_pattern_index < page_patterns_per_key; ++page_pattern_index) {
      // Page hint for "/news/"
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern(
          "/news" + base::NumberToString(page_pattern_index) + "/");
      optimization_guide::proto::Optimization* optimization1 =
          page_hint->add_whitelisted_optimizations();
      optimization1->set_optimization_type(
          optimization_guide::proto::RESOURCE_LOADING);

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_1 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_1->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_1->set_resource_pattern("news_cruft_1.js");

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_2 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_2->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_2->set_resource_pattern("news_cruft_2.js");
    }
  }

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::InitializeWithLitePageRedirectBlacklist() {
  optimization_guide::BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add("blacklisteddomain.com");
  blacklist_bloom_filter.Add("blacklistedsubdomain.maindomain.co.in");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(511);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ProcessHints(config, "2.0.0");
}

bool PreviewsOptimizationGuideTest::
    MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
        PreviewsUserData* previews_data,
        const GURL& url,
        PreviewsType type,
        net::EffectiveConnectionType* out_ect_threshold) {
  // Ensure that all asynchronous MaybeLoadOptimizationHints processing
  // finishes prior to calling IsWhitelisted. This is accomplished by waiting
  // for the OnLoadOptimizationHints callback to set |requested_hints_loaded_|
  // to true.
  requested_hints_loaded_ = false;
  if (guide()->MaybeLoadOptimizationHints(
          url, base::BindOnce(
                   &PreviewsOptimizationGuideTest::OnLoadOptimizationHints,
                   base::Unretained(this)))) {
    while (!requested_hints_loaded_) {
      RunUntilIdle();
    }
  }

  return guide()->IsWhitelisted(previews_data, url, type, out_ect_threshold);
}

void PreviewsOptimizationGuideTest::OnLoadOptimizationHints() {
  requested_hints_loaded_ = true;
}

TEST_F(PreviewsOptimizationGuideTest, IsWhitelistedWithoutHints) {
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForNoScriptPageHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Configure somedomain.org with 2 page patterns, different ECT thresholds.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("noscript_3g");
  page_hint2->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Configure anypage.com with * page pattern.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("anypage.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Verify page matches and ECT thresholds.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_3g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G, ect_threshold);
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/no_pattern_match"),
      PreviewsType::NOSCRIPT, &ect_threshold));

  // Verify * matches any page.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com/noscript_for_all"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com/"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithValidCommandLineOverride) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndGuide();

  // Verify page matches and ECT thresholds.
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  EXPECT_TRUE(guide()->GetHintsForTesting());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithValidCommandLineOverrideForDeferAllScript) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures({features::kDeferAllScriptPreviews}, {});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("defer_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(
      optimization_guide::proto::DEFER_ALL_SCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndGuide();

  // Verify page matches and ECT thresholds.
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  EXPECT_TRUE(guide()->GetHintsForTesting());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/defer_default_2g"),
      PreviewsType::DEFER_ALL_SCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithValidCommandLineOverrideAndPreexistingData) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("otherdomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndGuide();

  // Verify page matches and ECT thresholds.
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  EXPECT_TRUE(guide()->GetHintsForTesting());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://otherdomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithInvalidCommandLineOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->GetHintsForTesting());
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithPurgeHintCacheStoreCommandLineAndNoPreexistingData) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kPurgeHintCacheStore);
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithPurgeHintCacheStoreCommandLineAndPreexistingData) {
  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kPurgeHintCacheStore);
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

// Test when resource loading hints are enabled.
TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  // Add first hint.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization1->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add an additional optimization to first hint to verify that the applicable
  // optimizations are still whitelisted.
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint2 =
      optimization3->add_resource_loading_hints();
  resource_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2->set_resource_pattern("resource.js");

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

// Test when both NoScript and resource loading hints are enabled.
TEST_F(
    PreviewsOptimizationGuideTest,
    ProcessHintsWhitelistForNoScriptAndResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Add first hint.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add additional optimization to second hint to verify that the applicable
  // optimizations are still whitelisted.
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization2->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint2->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint2 =
      optimization3->add_resource_loading_hints();
  resource_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2->set_resource_pattern("resource.js");

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com/example.html"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForResourceLoadingHintsWithSlowPageTriggering) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  // Hint with 3G threshold.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("3g.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Hint with 4G threshold.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("4g.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  page_hint2->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_4G);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Hint with no threshold (default case).
  optimization_guide::proto::Hint* hint3 = config.add_hints();
  hint3->set_key("default2g.com");
  hint3->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint3->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://3g.com"), PreviewsType::RESOURCE_LOADING_HINTS,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://4g.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://default2g.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
}

// This is a helper function for testing the experiment flags on the config for
// the optimization guide. It creates a test config with a hint containing
// multiple optimizations. The optimization under test will be marked with an
// experiment name if one is provided in |experiment_name|. It will then be
// tested to see if it's enabled, the expectation found in |expect_enabled|.
void PreviewsOptimizationGuideTest::DoExperimentFlagTest(
    base::Optional<std::string> experiment_name,
    bool expect_enabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Create a hint with two optimizations. One may be marked experimental
  // depending on test configuration. The other is never marked experimental.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  // NOSCRIPT is the optimization under test and may be marked experimental.
  if (experiment_name.has_value()) {
    optimization1->set_experiment_name(experiment_name.value());
  }
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // RESOURCE_LOADING is not marked experimental.
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization2->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add a second, non-experimental hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Check to ensure the optimization under test (facebook noscript) is either
  // enabled or disabled, depending on what the caller told us to expect.
  EXPECT_EQ(expect_enabled, MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
                                &user_data, GURL("https://m.facebook.com"),
                                PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);

  // RESOURCE_LOADING_HINTS for facebook should always be enabled.
  ect_threshold = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  EXPECT_EQ(!expect_enabled,
            MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
                &user_data, GURL("https://m.facebook.com"),
                PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  // Twitter's NOSCRIPT should always be enabled; RESOURCE_LOADING_HINTS is not
  // configured and should be disabled.
  ect_threshold = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  // Google (which is not configured at all) should always have both NOSCRIPT
  // and RESOURCE_LOADING_HINTS disabled.
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithNoExperimentFlaggedOrEnabled) {
  // With the optimization NOT flagged as experimental and no experiment
  // enabled, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHintsExperiments);
  DoExperimentFlagTest(base::nullopt, true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithEmptyExperimentName) {
  // Empty experiment names should be equivalent to no experiment flag set.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndNotRunning) {
  // With the optimization flagged as experimental and no experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndSameOneRunning) {
  // With the optimization flagged as experimental and an experiment with that
  // name running, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "foo_experiment"}});
  DoExperimentFlagTest("foo_experiment", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndDifferentOneRunning) {
  // With the optimization flagged as experimental and a *different* experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "bar_experiment"}});
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, EnsureExperimentsDisabledByDefault) {
  // Mark an optimization as experiment, and ensure it's disabled even though we
  // don't explicitly enable or disable the feature as part of the test. This
  // ensures the experiments feature is disabled by default.
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsUnsupportedKeyRepIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(
      optimization_guide::proto::REPRESENTATION_UNSPECIFIED);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsUnsupportedOptimizationIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithExistingSentinel) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file for version 2.0.0.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "2.0.0", 5);

  // Verify config not processed for version 2.0.0 (same as sentinel).
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                           kFailedFinishProcessing),
      1);

  // Now verify config is processed for different version and sentinel cleared.
  ProcessHints(config, "3.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithInvalidSentinelFile) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file with invalid contents.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "bad-2.0.0", 5);

  // Verify config not processed for existing sentinel with bad value but
  // that the existinel sentinel file is deleted.
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                           kFailedFinishProcessing),
      1);

  // Now verify config is processed with sentinel cleared.
  ProcessHints(config, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      1);
}

TEST_F(PreviewsOptimizationGuideTest, SkipHintProcessingForSameConfigVersion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      1);

  // Verify hint config with the same version as the previously processed config
  // is skipped.
  ProcessHints(config2, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                           kSkippedProcessingHints),
      1);
}

TEST_F(PreviewsOptimizationGuideTest,
       SkipHintProcessingForEarlierConfigVersion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      1);

  // Verify hint config with an earlier version than the previously processed
  // one is skipped.
  ProcessHints(config2, "1.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                           kSkippedProcessingHints),
      1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessMultipleNewConfigs) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      1);

  // Verify hint config with a newer version then the previously processed one
  // is processed.
  ProcessHints(config2, "3.0.0");

  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(
          optimization_guide::ProcessHintsComponentResult::kSuccess),
      2);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintConfigWithNoKeyFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
  });
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsConfigWithDuplicateKeysFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("facebook.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
  });
}

TEST_F(PreviewsOptimizationGuideTest, MaybeLoadOptimizationHints) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithTwoExperimentsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithFirstExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_1"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithSecondExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_2"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithBothExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_1"},
       {"experiment_name", "experiment_2"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

// Test that optimization hints with multiple page patterns is processed
// correctly.
TEST_F(PreviewsOptimizationGuideTest,
       LoadManyResourceLoadingOptimizationHints) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  const size_t key_count = 50;
  const size_t page_patterns_per_key = 25;

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  InitializeMultipleResourceLoadingHints(key_count, page_patterns_per_key);

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/news0/football"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain49.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain49.org/news0/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain50.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain50.org/news0/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  RunUntilIdle();
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadOptimizationHintsWithoutEnabledPageHintsFeature) {
  // Without PageHints-oriented feature enabled, never see
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest, PreviewsUserDataPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(base::nullopt, user_data.serialized_hint_version_string());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ("someversion", user_data.serialized_hint_version_string());
}

TEST_F(PreviewsOptimizationGuideTest, IsBlacklisted) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_DCHECK_DEATH(guide()->IsBlacklisted(
      GURL("https://m.blacklisteddomain.com/path"), PreviewsType::NOSCRIPT));

  EXPECT_TRUE(guide()->IsBlacklisted(
      GURL("https://blacklistedsubdomain.maindomain.co.in"),
      PreviewsType::LITE_PAGE_REDIRECT));

  EXPECT_FALSE(guide()->IsBlacklisted(GURL("https://maindomain.co.in"),
                                      PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest,
       IsBlacklistedWithLitePageServerPreviewsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kLitePageServerPreviews);

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest, RemoveObserverCalledAtDestruction) {
  EXPECT_FALSE(optimization_guide_service()->RemoveObserverCalled());

  ResetGuide();

  EXPECT_TRUE(optimization_guide_service()->RemoveObserverCalled());
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherEnabledNoHosts) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_)).Times(1);

  // Load hints so that OnHintsUpdated is called. This will force FetchHints to
  // be triggered if OptimizationHintsFetching is enabled.
  InitializeFixedCountResourceLoadingHints();
  // Cause the timer to fire the fetch event.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}

TEST_F(PreviewsOptimizationGuideTest,
       HintsFetcherEnabledWithHostsNoHintsInResponse) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  std::vector<std::string> hosts = {"example1.com", "example2.com"};

  // This should be called exactly once, confirming that hints are not fetched
  // again after |kTestFetchRetryDelaySecs|.
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(hosts));

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithNoHints));

  // Load hints so that OnHintsUpdated is called. This will force FetchHints to
  // be triggered if OptimizationHintsFetching is enabled.
  InitializeFixedCountResourceLoadingHints();
  // Cause the timer to fire the fetch event.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());

  // Check that hints should not be fetched again after the delay for a failed
  // hints fetch attempt.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_)).Times(0);
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherEnabledWithHosts) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  std::vector<std::string> hosts = {"example1.com", "example2.com"};
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(hosts));

  // Load hints so that OnHintsUpdated is called. This will force FetchHints to
  // be triggered if OptimizationHintsFetching is enabled.
  InitializeFixedCountResourceLoadingHints();

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherTimerRetryDelay) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  std::string opt_guide_url = "https://hintsserver.com";

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchFailed));

  std::vector<std::string> hosts = {"example1.com", "example2.com"};
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Return(hosts));

  // Force hints fetch scheduling.
  guide()->ScheduleHintsFetch();

  // Force timer to expire and schedule a hints fetch - first time.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());

  // Force speculative timer to expire after fetch fails first time, update
  // hints fetcher so it succeeds this time.
  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherTimerFetchSucceeds) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  std::string opt_guide_url = "https://hintsserver.com";

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  std::vector<std::string> hosts = {"example1.com", "example2.com"};
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_))
      .WillRepeatedly(testing::Return(hosts));

  // Force hints fetch scheduling.
  guide()->ScheduleHintsFetch();

  // Force timer to expire and schedule a hints fetch that succeeds.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());

  // TODO(mcrouse): Make sure timer is triggered by metadata entry,
  // |hint_cache| control needed.
  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));

  RunUntilFetchedHintsStored();
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_)).Times(0);
  CreateServiceAndGuide();
  // Load hints so that OnHintsUpdated is called. This will
  // check that FetcHints is not triggered by making sure that top_host_provider
  // is not called.
  InitializeFixedCountResourceLoadingHints();
}

TEST_F(PreviewsOptimizationGuideTest, HintsFetcherLastFetchAtttempt) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  // Set the last fetch attempt to 5 minutes ago, simulating a short duration
  // since last execution (simulating browser crash/close-reopen).
  base::TimeDelta minutes_since_attempt = base::TimeDelta::FromMinutes(5);
  base::Time last_attempt_time = GetMockClock()->Now() - minutes_since_attempt;

  pref_service()->SetInt64(
      optimization_guide::prefs::kHintsFetcherLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  std::vector<std::string> hosts = {"example1.com", "example2.com"};
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(hosts));

  // Load hints so that OnHintsUpdated is called. This will force FetchHints to
  // be triggered if OptimizationHintsFetching is enabled.
  InitializeFixedCountResourceLoadingHints();

  // Move the clock forward a short duration to ensure that the hints fetch does
  // not occur too quickly after the simulated short relaunch.
  MoveClockForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());

  // Move clock forward by the maximum delay, |kTestFetchRetryDelaySeconds to
  // trigger the hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

class PreviewsOptimizationGuideDataSaverOffTest
    : public PreviewsOptimizationGuideTest {
 public:
  PreviewsOptimizationGuideDataSaverOffTest() {}

  ~PreviewsOptimizationGuideDataSaverOffTest() override {}

  void SetUp() override {
    PreviewsOptimizationGuideTest::SetUp();
    DisableDataSaver();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideDataSaverOffTest);
};

TEST_F(PreviewsOptimizationGuideDataSaverOffTest,
       HintsFetcherEnabledDataSaverDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  std::string opt_guide_url = "https://hintsserver.com";

  guide()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  std::vector<std::string> hosts = {"example1.com", "example2.com"};
  EXPECT_CALL(*top_host_provider(), GetTopHosts(testing::_)).Times(0);

  // Load hints so that OnHintsUpdated is called. This will force FetchHints to
  // be triggered if OptimizationHintsFetching is enabled.
  InitializeFixedCountResourceLoadingHints();

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}
}  // namespace previews
